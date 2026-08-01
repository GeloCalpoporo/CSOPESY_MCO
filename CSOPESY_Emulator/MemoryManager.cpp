#include "MemoryManager.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
    constexpr const char* BACKING_STORE_FILE = "csopesy-backing-store.txt";
    constexpr unsigned long long FLUSH_EVERY = 100;   // throttle: ~1 write per 100 ticks
}

// ----- setup -----
void MemoryManager::initialize(std::size_t maxOverallMem, std::size_t memPerFrame_) {
    std::lock_guard<std::mutex> lock(mtx);

    maxMem      = maxOverallMem;
    memPerFrame = (memPerFrame_ == 0) ? 1 : memPerFrame_;

    std::size_t frameCount = maxMem / memPerFrame;
    if (frameCount == 0) frameCount = 1;             // degenerate config: at least one frame

    physical.assign(frameCount * memPerFrame, 0);
    frames.assign(frameCount, Frame{});

    freeFrames.clear();
    for (std::size_t i = 0; i < frameCount; ++i) freeFrames.push_back(i);

    fifo.clear();
    tables.clear();
    store.clear();
    storeOwnerName.clear();

    numPagedIn = numPagedOut = 0;
    storeDirty = true;
    writeStoreFile();                                // start from a clean, existing file
    storeDirty = false;
}

void MemoryManager::registerProcess(int pid, std::size_t memBytes) {
    std::lock_guard<std::mutex> lock(mtx);

    PageTable pt;
    pt.memBytes = memBytes;
    pt.numPages = (memBytes + memPerFrame - 1) / memPerFrame;
    if (pt.numPages == 0) pt.numPages = 1;
    pt.frameOf.assign(pt.numPages, -1);              // nothing resident yet: pure demand paging
    tables[pid] = std::move(pt);
}

void MemoryManager::setProcessName(int pid, const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx);
    storeOwnerName[pid] = name;
}

// Memory is reclaimed only here - a process keeps its variables until it finishes.
void MemoryManager::releaseProcess(int pid) {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = tables.find(pid);
    if (it == tables.end()) return;

    for (std::size_t vp = 0; vp < it->second.numPages; ++vp) {
        long long f = it->second.frameOf[vp];
        if (f < 0) continue;
        std::size_t fi = static_cast<std::size_t>(f);
        frames[fi] = Frame{};
        freeFrames.push_back(fi);
        fifo.erase(std::remove(fifo.begin(), fifo.end(), fi), fifo.end());
    }
    tables.erase(it);

    for (auto sit = store.begin(); sit != store.end(); ) {
        if (sit->first.first == pid) { sit = store.erase(sit); storeDirty = true; }
        else                         { ++sit; }
    }
    storeOwnerName.erase(pid);
}

// ----- the demand pager -----
long long MemoryManager::takeVictimFrame() {
    // FIFO: oldest loaded frame first, skipping frames pinned by the instruction
    // currently being serviced (prevents a process from evicting the very page it
    // just faulted in while collecting the rest of its operands), and skipping
    // frames loaded on this same tick (prevents one core from undoing the page-in
    // another core just paid for, which would look like progress but is not).
    for (auto it = fifo.begin(); it != fifo.end(); ++it) {
        std::size_t fi = *it;
        if (frames[fi].pid < 0 || frames[fi].pinned) continue;
        if (frames[fi].loadedTick == currentTick)    continue;

        Frame& victim = frames[fi];

        // Evict: the page's bytes go to the backing store, its owner loses residency.
        std::vector<std::uint8_t> page(physical.begin() + fi * memPerFrame,
                                       physical.begin() + (fi + 1) * memPerFrame);
        store[{victim.pid, victim.vpage}] = std::move(page);
        storeDirty = true;
        ++numPagedOut;

        auto tit = tables.find(victim.pid);
        if (tit != tables.end() && victim.vpage < tit->second.numPages)
            tit->second.frameOf[victim.vpage] = -1;

        victim = Frame{};
        fifo.erase(it);
        return static_cast<long long>(fi);
    }
    return -1;   // every frame is pinned - caller retries on the next tick
}

MemoryManager::Access MemoryManager::ensureResident(int pid, std::size_t addr, bool pin) {
    std::lock_guard<std::mutex> lock(mtx);

    auto tit = tables.find(pid);
    if (tit == tables.end()) return Access::FAILED;

    PageTable& pt = tit->second;
    std::size_t vpage = addr / memPerFrame;
    if (vpage >= pt.numPages) return Access::FAILED;   // caller should have caught this

    if (pt.frameOf[vpage] >= 0) {                      // already resident: no fault
        if (pin) frames[static_cast<std::size_t>(pt.frameOf[vpage])].pinned = true;
        return Access::HIT;
    }

    // ----- page fault -----
    long long frameIdx;
    if (!freeFrames.empty()) {
        frameIdx = static_cast<long long>(freeFrames.front());
        freeFrames.pop_front();
    } else {
        frameIdx = takeVictimFrame();
        if (frameIdx < 0) return Access::FAILED;
    }

    std::size_t fi     = static_cast<std::size_t>(frameIdx);
    std::size_t offset = fi * memPerFrame;

    auto sit = store.find({pid, vpage});
    if (sit != store.end()) {
        // Restore the page we previously evicted.
        std::copy(sit->second.begin(), sit->second.end(), physical.begin() + offset);
        store.erase(sit);
        storeDirty = true;
    } else {
        // First touch: uninitialised memory reads as 0 (spec).
        std::fill(physical.begin() + offset, physical.begin() + offset + memPerFrame, 0);
    }
    ++numPagedIn;

    frames[fi].pid        = pid;
    frames[fi].vpage      = vpage;
    frames[fi].pinned     = pin;
    frames[fi].loadedTick = currentTick;
    pt.frameOf[vpage]     = frameIdx;
    fifo.push_back(fi);

    return Access::FAULT_SERVICED;
}

void MemoryManager::setTick(unsigned long long tick) {
    std::lock_guard<std::mutex> lock(mtx);
    currentTick = tick;
}

void MemoryManager::unpinProcess(int pid) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& f : frames)
        if (f.pid == pid) f.pinned = false;
}

// ----- address translation & access -----
const std::uint8_t* MemoryManager::translate(int pid, std::size_t addr) const {
    auto tit = tables.find(pid);
    if (tit == tables.end()) return nullptr;

    const PageTable& pt = tit->second;
    std::size_t vpage = addr / memPerFrame;
    if (vpage >= pt.numPages || pt.frameOf[vpage] < 0) return nullptr;

    std::size_t fi = static_cast<std::size_t>(pt.frameOf[vpage]);
    return &physical[fi * memPerFrame + (addr % memPerFrame)];
}

std::uint8_t* MemoryManager::translate(int pid, std::size_t addr) {
    return const_cast<std::uint8_t*>(
        static_cast<const MemoryManager*>(this)->translate(pid, addr));
}

std::uint16_t MemoryManager::readU16(int pid, std::size_t addr) const {
    std::lock_guard<std::mutex> lock(mtx);
    const std::uint8_t* lo = translate(pid, addr);
    const std::uint8_t* hi = translate(pid, addr + 1);
    if (!lo || !hi) return 0;
    return static_cast<std::uint16_t>(*lo) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(*hi) << 8);
}

void MemoryManager::writeU16(int pid, std::size_t addr, std::uint16_t value) {
    std::lock_guard<std::mutex> lock(mtx);
    std::uint8_t* lo = translate(pid, addr);
    std::uint8_t* hi = translate(pid, addr + 1);
    if (!lo || !hi) return;
    *lo = static_cast<std::uint8_t>(value & 0xFF);
    *hi = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

// ----- statistics -----
std::size_t MemoryManager::totalMemory() const {
    std::lock_guard<std::mutex> lock(mtx);
    return frames.size() * memPerFrame;
}

std::size_t MemoryManager::usedMemory() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::size_t used = 0;
    for (const auto& f : frames) if (f.pid >= 0) used += memPerFrame;
    return used;
}

std::size_t MemoryManager::freeMemory() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::size_t used = 0;
    for (const auto& f : frames) if (f.pid >= 0) used += memPerFrame;
    return frames.size() * memPerFrame - used;
}

std::size_t MemoryManager::frameSize()   const { std::lock_guard<std::mutex> l(mtx); return memPerFrame; }
std::size_t MemoryManager::totalFrames() const { std::lock_guard<std::mutex> l(mtx); return frames.size(); }

std::size_t MemoryManager::processResidentBytes(int pid) const {
    std::lock_guard<std::mutex> lock(mtx);
    std::size_t bytes = 0;
    for (const auto& f : frames) if (f.pid == pid) bytes += memPerFrame;
    return bytes;
}

unsigned long long MemoryManager::pagedIn()  const { std::lock_guard<std::mutex> l(mtx); return numPagedIn;  }
unsigned long long MemoryManager::pagedOut() const { std::lock_guard<std::mutex> l(mtx); return numPagedOut; }

// ----- backing store file -----
void MemoryManager::flushBackingStore(bool force) {
    std::lock_guard<std::mutex> lock(mtx);
    // A forced flush always rewrites: the header carries the live paged-in/paged-out
    // counters, so "vmstat" and the file must never disagree.
    if (force) { writeStoreFile(); storeDirty = false; return; }
    if (!storeDirty) return;
    if ((++flushCounter % FLUSH_EVERY) != 0) return;
    writeStoreFile();
    storeDirty = false;
}

// caller holds mtx
void MemoryManager::writeStoreFile() const {
    std::ofstream out(BACKING_STORE_FILE, std::ios::trunc);
    if (!out.is_open()) return;

    out << "CSOPESY BACKING STORE\n";
    out << "---------------------------------------------------------------\n";
    out << "Frame size      : " << memPerFrame << " bytes\n";
    out << "Total frames    : " << frames.size() << "\n";
    out << "Pages swapped   : " << store.size() << "\n";
    out << "Pages paged in  : " << numPagedIn << "\n";
    out << "Pages paged out : " << numPagedOut << "\n";
    out << "---------------------------------------------------------------\n\n";

    if (store.empty()) {
        out << "(no pages currently in the backing store)\n";
        return;
    }

    for (const auto& entry : store) {
        int         pid   = entry.first.first;
        std::size_t vpage = entry.first.second;

        auto nit = storeOwnerName.find(pid);
        out << "PID " << pid
            << " (" << (nit == storeOwnerName.end() ? std::string("?") : nit->second) << ")"
            << "  page " << vpage << ":\n";

        const std::vector<std::uint8_t>& bytes = entry.second;
        std::ostringstream line;
        line << std::hex << std::setfill('0');
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            line << std::setw(2) << static_cast<unsigned>(bytes[i]);
            if ((i + 1) % 32 == 0) { out << "  " << line.str() << "\n"; line.str(""); }
            else                     line << ' ';
        }
        if (!line.str().empty()) out << "  " << line.str() << "\n";
        out << "\n";
    }
}
