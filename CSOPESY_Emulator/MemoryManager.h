#pragma once

#include <cstdint>
#include <cstddef>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

//
// MemoryManager: the MCO2 demand-paging allocator.
//
// Simulated physical memory is a flat byte array of `max-overall-mem` bytes, cut into
// frames of `mem-per-frame` bytes. Every process gets its own virtual address space of
// `process_memory_size` bytes, cut into pages of the same size. Pages are NOT resident
// at process creation - they are brought in only when the running process touches them
// (a page fault). When no frame is free, a FIFO victim is evicted to the backing store,
// a plain text file named "csopesy-backing-store.txt" that can be inspected at any time.
//
// Thread safety: every public method takes `mtx`. The Scheduler calls in while holding
// its own lock and the console calls in for stats, so the lock order is always
// Scheduler::mtx -> MemoryManager::mtx. This class never calls back into the Scheduler.
//
class MemoryManager {
public:
    // Result of touching an address. FAULT_SERVICED means the page was not resident,
    // a fault was handled, and the instruction must be RESTARTED (spec p.6).
    enum class Access { HIT, FAULT_SERVICED, FAILED };

    void initialize(std::size_t maxOverallMem, std::size_t memPerFrame);

    // Page-table lifecycle. releaseProcess frees frames AND backing store pages -
    // process memory is only reclaimed when the process finishes (spec).
    void registerProcess(int pid, std::size_t memBytes);
    void releaseProcess(int pid);

    // Make the page holding `addr` resident. `pin` protects it from being chosen as a
    // victim while the current instruction is still collecting its other pages.
    Access ensureResident(int pid, std::size_t addr, bool pin);
    void   unpinProcess(int pid);

    // The scheduler stamps the current CPU tick before running the cores. A frame
    // loaded on tick N is not a replacement candidate until tick N+1, so a core can
    // never evict a page another core faulted in during the same tick. Without this,
    // 16 cores sharing 1 frame would each "succeed" every tick by stealing the frame
    // from the core before them, reporting 100% CPU use while achieving nothing.
    void   setTick(unsigned long long tick);

    // Require residency (guaranteed by ensureResident) - reads/writes are little-endian
    // and may straddle a page boundary, so both bytes are translated separately.
    std::uint16_t readU16(int pid, std::size_t addr) const;
    void          writeU16(int pid, std::size_t addr, std::uint16_t value);

    // ----- statistics for process-smi / vmstat -----
    std::size_t totalMemory() const;
    std::size_t usedMemory()  const;   // resident frames * frame size
    std::size_t freeMemory()  const;
    std::size_t frameSize()   const;
    std::size_t totalFrames() const;
    std::size_t processResidentBytes(int pid) const;   // sums to usedMemory()
    unsigned long long pagedIn()  const;
    unsigned long long pagedOut() const;

    // Rewrites csopesy-backing-store.txt. Called cheaply (throttled) from the tick loop
    // and forced right before any command that shows memory, so the file on disk is
    // always current whenever a human looks at it.
    void flushBackingStore(bool force);

private:
    struct Frame {
        int                pid        = -1;      // -1 = free
        std::size_t        vpage      = 0;
        bool               pinned     = false;
        unsigned long long loadedTick = 0;       // tick this page was paged in on
    };

    struct PageTable {
        std::size_t             memBytes = 0;
        std::size_t             numPages = 0;
        std::vector<long long>  frameOf;      // -1 = not resident
    };

    // caller must hold mtx
    const std::uint8_t* translate(int pid, std::size_t addr) const;
    std::uint8_t*       translate(int pid, std::size_t addr);
    long long           takeVictimFrame();
    void                writeStoreFile() const;

    std::vector<std::uint8_t> physical;      // the simulated RAM
    std::vector<Frame>        frames;
    std::deque<std::size_t>   freeFrames;
    std::deque<std::size_t>   fifo;          // frame indices in load order (FIFO victim)

    std::unordered_map<int, PageTable> tables;

    // Swapped-out page contents. std::map keeps the text file in a stable, readable order.
    std::map<std::pair<int, std::size_t>, std::vector<std::uint8_t>> store;
    std::unordered_map<int, std::string> storeOwnerName;   // pid -> process name, for the file

    std::size_t memPerFrame = 0;
    std::size_t maxMem      = 0;

    unsigned long long currentTick = 0;   // set by the scheduler once per tick

    unsigned long long numPagedIn  = 0;
    unsigned long long numPagedOut = 0;

    bool               storeDirty = false;
    unsigned long long flushCounter = 0;

    mutable std::mutex mtx;

public:
    // Purely cosmetic: lets the backing store file label pages with the process name.
    void setProcessName(int pid, const std::string& name);
};
