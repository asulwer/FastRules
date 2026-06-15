# FastRules Concurrency Improvements - 3 Phase Plan

This document outlines a phased approach to improving concurrency in FastRules.

---

## Phase 1: Quick Wins (Immediate Benefits, Low Risk)

### 1.1 Read-Write Lock for Repository
**Priority:** High  
**Risk:** Low  
**Effort:** Small

**Current Issue:** DB repository may block on reads when multiple threads access rules simultaneously.

**Solution:** Replace mutex with `std::shared_mutex`:
- Multiple concurrent readers
- Exclusive writer access
- Prevents contention on read-heavy workloads

**Files to Modify:**
- `extensions/db/src/db_repository.cpp`

**Implementation:**
```cpp
// Replace:
std::mutex mutex_;

// With:
std::shared_mutex mutex_;
// Readers: std::shared_lock
// Writers: std::unique_lock
```

---

### 1.2 Async Rule Execution API
**Priority:** High  
**Risk:** Low  
**Effort:** Medium

**Current Issue:** All execution is synchronous, blocking the calling thread.

**Solution:** Add `executeAsync()` method returning `std::future<RuleResult>`:

```cpp
// New API:
std::future<std::vector<RuleResult>> executeAsync(
    Engine& engine,
    const std::vector<RuleParameter>& params
);
```

**Benefits:**
- Non-blocking workflow execution
- Natural integration with async/await patterns
- Caller can process results when ready

**Files to Modify:**
- `include/fastrules/workflow.hpp`
- `src/workflow.cpp`

---

## Phase 2: Architecture Improvements (Medium Risk, High Impact)

### 2.1 Lock-Free Rule Scheduling
**Priority:** Medium  
**Risk:** Medium  
**Effort:** Large

**Current Issue:** Thread pool with locks may become bottleneck at high concurrency.

**Solution:** Implement work-stealing queue:
- Each thread has local task queue
- Idle threads steal from busy threads
- Lock-free operations for queue access

**Implementation Options:**
1. **Intel TBB** (if dependency acceptable)
2. **Custom implementation** using atomics

**Files to Modify:**
- `include/fastrules/async_workflow.hpp`
- `src/async_workflow.cpp`
- New file: `include/fastrules/work_stealing_queue.hpp`

**Benchmark:** Compare throughput vs current thread pool at 1000+ concurrent rules.

---

### 2.2 Concurrent Workflow Compilation
**Priority:** Medium  
**Risk:** Medium  
**Effort:** Medium

**Current Issue:** Workflow compilation is single-threaded, sequential.

**Solution:** Compile independent rules in parallel:
- Analyze rule dependency graph
- Compile independent rules concurrently
- Merge compiled results

**When to Enable:**
- Workflows with 10+ rules
- Rules have no dependencies on each other

**Files to Modify:**
- `src/workflow.cpp` (compile method)
- New analysis phase for dependency graph

---

## Phase 3: Modern C++ (High Risk, Transformative)

### 3.1 Coroutines (C++20)
**Priority:** Low (Future)  
**Risk:** High  
**Effort:** Very Large

**Current Issue:** Thread-based parallelism has overhead.

**Solution:** Use C++20 coroutines for async rule chains:

```cpp
// Example API:
task<RuleResult> executeCoroutine(Engine& engine);

// Usage:
auto result = co_await workflow.executeCoroutine(engine);
```

**Benefits:**
- Millions of concurrent "tasks" (vs thousands of threads)
- Zero-cost abstraction for async code
- Natural sequential-looking async code

**Requirements:**
- C++20 compiler support
- Complete API redesign
- Breaking changes to public API

**Files to Modify:**
- Core API redesign required
- All async methods

**Timeline:** Major version release (2.0)

---

## Recommended Implementation Order

1. **Week 1:** Phase 1.1 (Read-Write Lock)
2. **Week 2-3:** Phase 1.2 (Async Execution)
3. **Month 2:** Phase 2.1 (Lock-Free Scheduling)
4. **Month 3:** Phase 2.2 (Concurrent Compilation)
5. **Future:** Phase 3 (Coroutines) - when C++20 is baseline

---

## Success Metrics

Track these metrics to measure improvement:

| Metric | Current | Target Phase 1 | Target Phase 2 | Target Phase 3 |
|--------|---------|----------------|----------------|----------------|
| Rules/sec (1 thread) | Baseline | +5% | +20% | +50% |
| Rules/sec (8 threads) | Baseline | +10% | +40% | +100% |
| Memory per concurrent rule | Baseline | Same | -30% | -80% |
| Latency (p99) | Baseline | -20% | -40% | -60% |

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Performance regression | Benchmark before/after each phase |
| Thread safety bugs | Stress tests with ThreadSanitizer |
| API breakage | Maintain backward compatibility in Phase 1-2 |
| Build complexity | Feature flags for new concurrency features |

---

*Created: 2026-06-15*  
*Status: Proposal - Pending Review*
