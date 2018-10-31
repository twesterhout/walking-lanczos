// Copyright Tom Westerhout (c) 2018
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//
//     * Neither the name of Tom Westerhout nor the names of other
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "random.hpp"
#include "spin_chain.hpp"
#include <complex>
#include <iosfwd>
#include <mutex>
#include <optional>
#include <random>
#include <thread>
#include <vector>

#include <boost/lockfree/spsc_queue.hpp>
#include <flat_hash_map/bytell_hash_map.hpp>

struct SpinHasher {
    auto operator()(SpinVector x) const noexcept { return x.hash(); }
};

class QuantumState {

  public:
    using map_type =
        ska::bytell_hash_map<SpinVector, std::complex<double>, SpinHasher>;
    using value_type = map_type::value_type;

  private:
    std::vector<map_type> _maps;
    std::byte             _index_mask;
    std::size_t           _soft_max_size;
    std::size_t           _hard_max_size;
    bool                  _use_random_sampling;

  public:
    QuantumState(std::size_t const soft_max, std::size_t const hard_max,
        std::size_t const number_workers, bool const use_random_sampling)
        : _maps{}
        , _index_mask{}
        , _soft_max_size{soft_max}
        , _hard_max_size{hard_max}
        , _use_random_sampling{use_random_sampling}
    {
        TCM_ASSERT(number_workers != 0 && (number_workers & (number_workers - 1)) == 0);
        if (soft_max < 2u) {
            throw_with_trace(
                std::invalid_argument{"`soft_max` must be at least 2."});
        }
        // Because of spin_to_index
        if (number_workers > (1u << 8)) {
            throw_with_trace(std::invalid_argument{
                "Currently, at most 64 workers can be used."});
        }

        _index_mask = std::byte{number_workers - 1u};
        _maps.reserve(number_workers);
        for (std::size_t i = 0; i < number_workers; ++i) {
            _maps.emplace_back(hard_max);
        }
    }

    QuantumState(QuantumState const&) = delete;
    QuantumState(QuantumState&&)      = default;
    QuantumState& operator=(QuantumState const&) = delete;
    QuantumState& operator=(QuantumState&&) = default;

    BOOST_FORCEINLINE
    constexpr auto spin_to_index(SpinVector const& spin) const noexcept
        -> std::size_t
    {
        return std::to_integer<std::size_t>(*spin.data() & _index_mask);
    }

    auto clear() -> void;
    auto insert(value_type &&) -> std::pair<map_type::iterator, bool>;

    auto find(SpinVector const& spin) -> std::optional<map_type::iterator>
    {
        auto& table = _maps.at(spin_to_index(spin));
        auto  where = table.find(spin);
        if (where != table.end()) { return {where}; }
        return std::nullopt;
    }

    auto shrink() -> void;
    auto normalize() -> QuantumState&;

    constexpr auto soft_max() const noexcept { return _soft_max_size; }
    constexpr auto hard_max() const noexcept { return _hard_max_size; }
    constexpr auto uses_random_sampling() const noexcept
    {
        return _use_random_sampling;
    }
    auto number_workers() const noexcept { return _maps.size(); }

    auto estimate_hard_max() const noexcept -> std::size_t
    {
        using std::begin, std::end;
        return std::max_element(begin(_maps), end(_maps),
            [](auto const& x, auto const& y) {
                return x.bucket_count() < y.bucket_count();
            })
            ->bucket_count();
    }

    constexpr auto& tables() & noexcept { return _maps; }

    template <class Function>
    auto for_each(Function&&) const -> void;

    friend auto operator>>(std::istream&, QuantumState&) -> std::istream&;
    friend auto operator<<(std::ostream&, QuantumState const&) -> std::ostream&;

  private:
    auto remove_least(std::size_t count) -> void;
    auto random_resample(std::size_t count, RandomGenerator& gen) -> void;
};

auto print(std::FILE*, QuantumState const&) -> void;
auto read_state(std::FILE*, QuantumState&) -> void;

template <class Function>
BOOST_FORCEINLINE auto QuantumState::for_each(Function&& fn) const -> void
{
    for (auto const& table : _maps) {
        for (auto& x : table) {
            fn(x);
        }
    }
}

class Updater {
    using value_type = QuantumState::value_type;
    using queue_type = boost::lockfree::spsc_queue<value_type,
        boost::lockfree::capacity<1024>>;
    using map_type   = QuantumState::map_type;

  public:
    Updater(map_type& table)
        : _table{std::addressof(table)}, _queue{}, _done{true}, _worker{}
    {
    }

  private:
    auto unsafe_process(value_type value)
    {
        auto where = _table->find(value.first);
        if (where == _table->end()) { _table->insert(value); }
        else {
            where->second += value.second;
        }
    }

  public:
    auto start()
    {
        TCM_ASSERT(_done);
        TCM_ASSERT(_queue.empty());
        TCM_ASSERT(!_worker.joinable());
        _done   = false;
        _worker = std::thread{[this]() {
            value_type x;
            while (!_done) {
                while (_queue.pop(x))
                    unsafe_process(x);
            }
            while (_queue.pop(x))
                unsafe_process(x);
        }};
    }

    auto stop()
    {
        TCM_ASSERT(!_done);
        TCM_ASSERT(_worker.joinable());
        _done = true;
        _worker.join();
        TCM_ASSERT(_queue.empty());
    }

    auto operator()(std::pair<SpinVector, std::complex<double>> value) -> void
    {
        TCM_ASSERT(!_done); // if (_done) { start(); }
        while (!_queue.push(value))
            ;
    }

  private:
    map_type*        _table;
    queue_type       _queue;
    std::atomic_bool _done;
    std::thread      _worker;
};

class QuantumStateBuilder {
    QuantumState&                         _psi;
    std::vector<std::unique_ptr<Updater>> _updaters;

  public:
    using value_type = QuantumState::value_type;

    QuantumStateBuilder(QuantumState& psi) : _psi{psi}, _updaters{}
    {
        TCM_ASSERT(
            psi.number_workers() > 0
            && ((psi.number_workers() & (psi.number_workers() - 1)) == 0));
        _updaters.reserve(psi.number_workers());
        for (auto& table : psi.tables()) {
            _updaters.emplace_back(std::make_unique<Updater>(table));
        }
    }

    QuantumStateBuilder(QuantumStateBuilder const&) = delete;
    QuantumStateBuilder(QuantumStateBuilder&&)      = delete;
    QuantumStateBuilder& operator=(QuantumStateBuilder const&) = delete;
    QuantumStateBuilder& operator=(QuantumStateBuilder&&) = delete;

    auto start() -> void
    {
        std::for_each(
            begin(_updaters), end(_updaters), [](auto& x) { x->start(); });
    }

    auto stop() -> void
    {
        std::for_each(
            begin(_updaters), end(_updaters), [](auto& x) { x->stop(); });
    }

    auto operator+=(std::pair<std::complex<double>, SpinVector> const& x)
        -> QuantumStateBuilder&
    {
        TCM_ASSERT(_updaters.size() < (1ul << 8));
        (*_updaters[_psi.spin_to_index(x.second)])({x.second, x.first});
        return *this;
    }
};

