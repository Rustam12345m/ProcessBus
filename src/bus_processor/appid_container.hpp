#pragma once

#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <climits>

/**
 * @brief Container for storing GOOSE/SV elements with unique a AppID
 *
 * Interface similar to std::unordered_map
 */
template< typename TKey, typename TValue >
class AppIdContainer
{
public:
    using Value = std::pair< TKey, TValue >;
    AppIdContainer() {
        std::fill(m_register.begin(), m_register.end(), SIZE_MAX);
    }

    bool empty() const { return m_values.empty(); }
    auto begin() { return m_values.begin(); }
    auto end() { return m_values.end(); }

    auto find(const TKey& key) {
        size_t idx = m_register[key.appid];
        if (idx < m_values.size() && m_values[idx].first == key) {
            return m_values.begin() + idx;
        }
        return m_values.end();
    }

    void insert(const TKey &key, const TValue &value) {
        m_register[key.appid] = m_values.size();
        m_values.emplace_back(key, value);
    }

    TValue& operator[](const TKey &key) {
        size_t idx = m_register[key.appid];
        if (idx >= m_values.size()) {
            insert(key, TValue());
            idx = m_register[key.appid];
        }
        return m_values[idx].second;
    }

private:
    std::array< size_t, USHRT_MAX > m_register;
    std::vector< Value >            m_values;
};

