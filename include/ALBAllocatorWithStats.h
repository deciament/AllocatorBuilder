///////////////////////////////////////////////////////////////////
//
// Copyright 2014 Felix Petriconi
//
// License: http://boost.org/LICENSE_1_0.txt, Boost License 1.0
//
// Authors: 
//          http://erdani.com, Andrei Alexandrescu
//          http://petriconi.net, Felix Petriconi
//
//////////////////////////////////////////////////////////////////
#pragma once

#include "ALBAllocatorBase.h"
#include "ALBAffixAllocator.h"
#include <chrono>
#include <boost/iterator/iterator_facade.hpp>

namespace ALB {

  /**
   * The following options define what statistics shall be collected during runtime
   * Directly taken from https://github.com/andralex/phobos/blob/allocator/std/allocator.d
   */
  enum StatsOptions : unsigned { 
    /**
     * Counts the number of calls to #owns).
     */
    numOwns = 1u << 0,
    /**
     * Counts the number of calls to #allocate. All calls are counted,
     * including requests for zero bytes or failed requests.
    */
    numAllocate = 1u << 1,
    /**
     * Counts the number of calls to #allocate that succeeded, i.e. they were
     * for more than zero bytes and returned a non-null block.
     */
    numAllocateOK = 1u << 2,
    /**
     * Counts the number of calls to #expand, regardless of arguments or
     * result.
     */
    numExpand = 1u << 3,
    /**
     * Counts the number of calls to #expand that resulted in a successful
     * expansion.
     */
    numExpandOK = 1u << 4,
    /**
     * Counts the number of calls to #reallocate, regardless of arguments or
     * result.
     */
    numReallocate = 1u << 5,
    /**
     * Counts the number of calls to #reallocate that succeeded. (Reallocations
     * to zero bytes count as successful.)
     */
    numReallocateOK = 1u << 6,
    /**
     * Counts the number of calls to #reallocate that resulted in an in-place
     * reallocation (no memory moved). If this number is close to the total number
     * of reallocations, that indicates the allocator finds room at the current
     * block's end in a large fraction of the cases, but also that internal
     * fragmentation may be high (the size of the unit of allocation is large
     * compared to the typical allocation size of the application).
     */
    numReallocateInPlace = 1u << 7,
    /**
     * Counts the number of calls to #deallocate.
     */
    numDeallocate = 1u << 8,
    /**
     * Counts the number of calls to #deallocateAll.
     */
    numDeallocateAll = 1u << 9,
    /**
     * Chooses all numXxx)flags.
     */
    numAll = (1u << 10) - 1,
    /**
     * Tracks total cumulative bytes allocated by means of #allocate,
     * #expand, and #reallocate (when resulting in an expansion). This
     * number always grows and indicates allocation traffic. To compute bytes
     * currently allocated, subtract #bytesDeallocated (below) from
     * #bytesAllocated.
     */
    bytesAllocated = 1u << 10,
    /**
     * Tracks total cumulative bytes deallocated by means of #deallocate and
     * #reallocate (when resulting in a contraction). This number always grows
     * and indicates deallocation traffic.
     */
    bytesDeallocated = 1u << 11,
    /**
     * Tracks the sum of all @delta values in calls of the form
     * #expand(b, delta)) that succeed (return @return).
     */
    bytesExpanded = 1u << 12,
    /**
     * Tracks the sum of all (b.length - s) with (b.length > s) in calls of
     * the form #reallocate(b, s)) that succeed (return $(D true)).
     */
    bytesContracted = 1u << 13,
    /**
     * Tracks the sum of all bytes moved as a result of calls to @reallocate that
     * were unable to reallocate in place. A large number (relative to 
     * bytesAllocated)) indicates that the application should use larger
     * preallocations.
     */
    bytesMoved = 1u << 14,
    /**
     * Measures the sum of extra bytes allocated beyond the bytes requested, i.e.
     * the $(WEB goo.gl/YoKffF, internal fragmentation). This is the current
     * effective number of slack bytes, and it goes up and down with time.
     */
    bytesSlack = 1u << 15,
    /**
     * Measures the maximum bytes allocated over the time. This is useful for
     * dimensioning allocators.
     */
    bytesHighTide = 1u << 16,
    /**
     * Chooses all byteXxx flags.
     */
    bytesAll = ((1u << 17) - 1) & ~numAll,
    /**
     * Instructs AllocatorWithStats to store the size asked by the caller for
     * each allocation. All per-allocation data is stored just before the actually
     * allocation AffixAllocator.
    */
    callerSize = 1u << 17,
    /**
     * Instructs AllocatorWithStats to store the caller's file for each
     * allocation.
     */
    callerFile = 1u << 18,
    /**
     * Instructs AllocatorWithStats to store the caller __FUNCTION__ for
     * each allocation.
     */
    callerFunction = 1u << 19,
    /**
     * Instructs AllocatorWithStats to store the caller's line for each
     * allocation.
     */
    callerLine = 1u << 20,
    /**
     * Instructs AllocatorWithStats to store the time of each allocation.
     */
    callerTime = 1u << 21,
    /**
     * Chooses all callerXxx flags.
     */
    callerAll = ((1u << 22) - 1) & ~numAll & ~bytesAll,
    /**
     * Combines all flags above.
     */
    all = (1u << 22) - 1      
  };

/// Use this macro if you want to store the caller information
#define ALLOCATE_WITH_CALLER_INFO(A, N) A.allocate(N, __FILE__, __FUNCTION__, __LINE__)

/// Simple way to define member and accessors
#define MEMBER_ACCESSOR(X)          \
    private:                        \
      size_t  _##X;                 \
    public:                         \
      size_t X() const { return _##X; }


  /**
   * This Allocator serves as a facade in front of the specified allocator to collect
   * statistics during runtime about all operations done on this instance.
   * Be aware that collecting of caller informations can easily consume lot's
   * of memory! With a good optimizing compiler only the code for the enabled
   * statistic information gets created.
   * @tparam Allocator The allocator that performs all allocations
   * @tparam Flags Specifies what kind of statistics get collected
   * \ingroup group_allocators
   */
  template <class Allocator, unsigned Flags = ALB::StatsOptions::all>
  class AllocatorWithStats {
    // in case that we store allocation state, we use an AffixAllocator to store
    // the additional informations as a Prefix
  public:
    struct AllocationInfo;
  private:
    static const bool HasPerAllocationState =
      (Flags & (StatsOptions::callerTime |
      StatsOptions::callerFile |
      StatsOptions::callerLine)) != 0;

    typename Traits::type_switch<
      AffixAllocator<Allocator, AllocationInfo>, 
      Allocator, 
      HasPerAllocationState>::type _allocator;

    AllocationInfo* _root;

#define MEMBER_ACCESSORS                   \
    MEMBER_ACCESSOR(numOwns)               \
    MEMBER_ACCESSOR(numAllocate)           \
    MEMBER_ACCESSOR(numAllocateOK)         \
    MEMBER_ACCESSOR(numExpand)             \
    MEMBER_ACCESSOR(numExpandOK)           \
    MEMBER_ACCESSOR(numReallocate)         \
    MEMBER_ACCESSOR(numReallocateOK)       \
    MEMBER_ACCESSOR(numReallocateInPlace)  \
    MEMBER_ACCESSOR(numDeallocate)         \
    MEMBER_ACCESSOR(numDeallocateAll)      \
    MEMBER_ACCESSOR(bytesAllocated)        \
    MEMBER_ACCESSOR(bytesDeallocated)      \
    MEMBER_ACCESSOR(bytesExpanded)         \
    MEMBER_ACCESSOR(bytesContracted)       \
    MEMBER_ACCESSOR(bytesMoved)            \
    MEMBER_ACCESSOR(bytesSlack)            \
    MEMBER_ACCESSOR(bytesHighTide)


  MEMBER_ACCESSORS

#undef MEMBER_ACCESSOR
#undef MEMBER_ACCESSORS

    template <typename T>
    void up(ALB::StatsOptions option, T& value) {
      if (Flags & option)
        value++;
    }
    template <typename T>
    void upOK(ALB::StatsOptions option, T& value, bool ok) {
      if (Flags & option && ok)
        value++;
    }
    template <typename T>
    void add(ALB::StatsOptions option, T& value, typename std::make_signed<T>::type delta) {
      if (Flags & option)
        value += delta;
    }
    template <typename T>
    void set(ALB::StatsOptions option, T& value, T t) {
      if (Flags & option)
        value = std::move(t);
    }


    void updateHighTide() {
      if (Flags & StatsOptions::bytesHighTide)
      {
        const size_t currentlyAllocated = _bytesAllocated - _bytesDeallocated;
        if (_bytesHighTide < currentlyAllocated) {
          _bytesHighTide = currentlyAllocated;
        }
      }
    }

  public:
    static const bool supports_truncated_deallocation = Allocator::supports_truncated_deallocation;
    static const bool has_per_allocation_state = HasPerAllocationState;

    struct AllocationInfo {
      size_t        callerSize;
      const char*   callerFile;
      const char*   callerFunction;
      int           callerLine;
      std::chrono::time_point<std::chrono::system_clock> callerTime;
      AllocationInfo* previous, *next;
    };

    class AllocationInfoIterator : public boost::iterator_facade<
      AllocationInfoIterator
      , AllocationInfo
      , boost::forward_traversal_tag
    > {
      friend class boost::iterator_core_access;

      void increment() { _node = _node->next; }

      bool equal(AllocationInfoIterator const& rhs) const { 
        return this->_node == rhs._node; 
      }

      AllocationInfo& dereference() const { return *_node; }

      AllocationInfo* _node;

    public:
      AllocationInfoIterator() : _node(nullptr) {}

      explicit AllocationInfoIterator(AllocationInfo* p) : _node(p) {}
    };    

    class Allocations {
      const AllocationInfoIterator _begin;
      const AllocationInfoIterator _end;

    public:
      Allocations(AllocationInfo* root) : _begin(root) {}
      typedef AllocationInfo value_type;
      typedef AllocationInfoIterator iterator;
      typedef AllocationInfoIterator const_iterator;
      const_iterator cbegin() const { return _begin; }
      const_iterator cend() const { return _end; }
      bool empty() const { return _begin == _end; }
    };
    
    AllocatorWithStats() 
      : _root(nullptr)
      , _numOwns(0)
      , _numAllocate(0)
      , _numAllocateOK(0)
      , _numExpand(0)
      , _numExpandOK(0)
      , _numReallocate(0)
      , _numReallocateOK(0)
      , _numReallocateInPlace(0)
      , _numDeallocate(0)
      , _numDeallocateAll(0)
      , _bytesAllocated(0)
      , _bytesDeallocated(0)
      , _bytesExpanded(0)
      , _bytesContracted(0)
      , _bytesMoved(0)
      , _bytesSlack(0)
      , _bytesHighTide(0)
    {}

    /**
     * The number of specified bytes gets allocated by the underlying Allocator.
     * Depending on the specified Flag, the allocating statistic information 
     * is stored.
     */
    Block allocate(size_t n, const char* file = nullptr, const char* function = nullptr, int line = 0) {
      auto result = _allocator.allocate(n);
      up(StatsOptions::numAllocate, _numAllocate);
      upOK(StatsOptions::numAllocateOK, _numAllocateOK, n > 0 && result);
      add(StatsOptions::bytesAllocated, _bytesAllocated, result.length);
      updateHighTide();
      if (result && has_per_allocation_state) {
        AllocationInfo* stat = Traits::AffixExtractor<decltype(_allocator), AllocationInfo>::prefix(_allocator, result);
        set(StatsOptions::callerSize, stat->callerSize, n);
        set(StatsOptions::callerFile, stat->callerFile, file);
        set(StatsOptions::callerFunction, stat->callerFunction, function);
        set(StatsOptions::callerLine, stat->callerLine, line);
        set(StatsOptions::callerTime, stat->callerTime, std::chrono::system_clock::now());
        if (_root) {
          _root->previous = stat;
          stat->next = _root;
          stat->previous = nullptr;
          _root = stat;
        }
        else {
          stat->previous = nullptr;
          stat->next = nullptr;
          _root = stat;
        }
      }
      return result;
    }
    /**
     * The specified block gets freed by the underlaying Allocator
     * Depending on the specified Flag, the deallocating statistic information
     * is stored.
     */
    void deallocate(Block& b) {
      up(StatsOptions::numDeallocate, _numDeallocate);
      add(StatsOptions::bytesDeallocated, _bytesDeallocated, b.length);

      if (b && has_per_allocation_state) {
        AllocationInfo* stat = Traits::AffixExtractor<decltype(_allocator), AllocationInfo>::prefix(_allocator, b);
        if (stat->previous) {
          stat->previous->next = stat->next;
        }
        if (stat->next){
          stat->next->previous = stat->previous;
        }
        if (stat == _root) _root = stat->previous;
      }
      _allocator.deallocate(b);
    }

    /**
     * The specified block gets reallocated by the underlaying Allocator
     * Depending on the specified Flag, the reallocating statistic information 
     * is stored.
     */
    bool reallocate(Block& b, size_t n) {
      auto originalBlock = b;
      up(StatsOptions::numReallocate, _numReallocate);

      if (!_allocator.reallocate(b, n))
      {
        return false;
      }
      up(StatsOptions::numReallocateOK, _numReallocateOK);
      std::make_signed<size_t>::type delta = b.length - originalBlock.length;
      if (b.ptr == originalBlock.ptr) {
        up(StatsOptions::numReallocateInPlace, _numReallocateInPlace);
        if (delta > 0) {
          add(StatsOptions::bytesAllocated, _bytesAllocated, delta);
          add(StatsOptions::bytesExpanded, _bytesExpanded, delta);
        }
        else {
          add(StatsOptions::bytesDeallocated, _bytesDeallocated, -delta);
          add(StatsOptions::bytesContracted, _bytesContracted, -delta);
        }
      }  // was moved to a new location
      else {
        add(StatsOptions::bytesAllocated, _bytesAllocated, b.length);
        add(StatsOptions::bytesMoved, _bytesMoved, originalBlock.length);
        add(StatsOptions::bytesDeallocated, _bytesDeallocated, originalBlock.length);
      }
      updateHighTide();
      return true;
    }

    /**
     * The given block is passed to the underlying Allocator to be checked
     * for ownership. 
     * This method is only available if the underlying Allocator implements it.
     * Depending on the specified Flag, the ownc statistic information 
     * is stored.
     */
    typename Traits::enable_result_to<bool, Traits::has_owns<Allocator>::value>::type 
    owns(const Block& b) const {
      up(StatsOptions::numOwns, _numOwns);
      return _allocator.owns(b);
    }

    /**
     * The given block is passed to the underlying Allocator to be expanded
     * This method is only available if the underlying allocator implements it.
     * Depending on the specified Flag, the expand statistic information 
     * is stored.
     */
    typename Traits::enable_result_to<bool, Traits::has_expand<Allocator>::value>::type 
    expand(Block& b, size_t delta) {
      up(StatsOptions::numExpand, _numExpand);
      auto oldLength = b.length;
      auto result = _allocator.expand(b, delta);
      if (result)
      {
        up(StatsOptions::numExpandOK, _numExpandOK);
        add(StatsOptions::bytesExpanded, _bytesExpanded, b.length - oldLength);
        add(StatsOptions::bytesAllocated, _bytesAllocated, b.length - oldLength);
        updateHighTide();
      }
      return result;
    }

    Allocations allocations() const {
      return Allocations(_root);
    }
  };
}