#include <gtest/gtest.h>

#include "bus_processor/appid_container.hpp"

TEST(AppIdContainer, BasicUsage)
{
    struct Key
    {
        uint16_t appid = 0;
        std::string goid;
    };
    struct Value
    {
        using ptr = std::shared_ptr< Value >;

        unsigned value = 0;
    };

    Key k[5] = { { 1, "GOID1" },
                 { 2, "GOID2" },
                 { 3, "GOID3" },
                 { 4, "GOID4" },
                 { 5, "GOID5" } };
    Value::ptr v[5] = { std::make_shared< Value >(),
                        std::make_shared< Value >(),
                        std::make_shared< Value >(),
                        std::make_shared< Value >(),
                        std::make_shared< Value >() };

    AppIdContainer< Key, Value::ptr > map;
    for (int i=0;i<5;++i) {
        map.insert(k[i], v[i]);
    }

    for (int i=0;i<5;++i) {
        ASSERT_EQ(map[k[i]], v[i]);
    }
}

