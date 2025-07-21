#include <gtest/gtest.h>
#include <format>

#include "bus_processor/pipeline.hpp"

enum EStages
{
    START_STAGE = 0,

    ROUTER = 0,
    SV,
    GOOSE,
    IP,

    STAGE_NUM
};

template< typename TMatrix >
struct RouterStage
{
    static void ApplyTo(TMatrix& matrix) {
        typename TMatrix::Frame &frame = matrix.stages[EStages::ROUTER];

        for (unsigned i=0;i<frame.num;++i) {
            unsigned proto = *(unsigned *)frame.buf[i];
            matrix.stages[proto].PutBuffer(frame.buf[i]);
        }

        matrix.app->total = frame.num;

        //! Clean frame for the next cycle
        frame.num = 0;
    }
};

template< typename TMatrix >
struct GooseStage
{
    static void ApplyTo(TMatrix &matrix) {
        typename TMatrix::Frame &frame = matrix.stages[EStages::GOOSE];

        for (unsigned i=0;i<frame.num;++i) {
            unsigned proto = *(unsigned *)frame.buf[i];
            if (proto == EStages::GOOSE) {
                matrix.app->goose++;
            }
        }

        //! Clean frame for the next cycle
        frame.num = 0;
    }
};

template< typename TMatrix >
struct SampledValuesStage
{
    static void ApplyTo(TMatrix &matrix) {
        typename TMatrix::Frame &frame = matrix.stages[EStages::SV];

        for (unsigned i=0;i<frame.num;++i) {
            unsigned proto = *(unsigned *)frame.buf[i];
            if (proto == EStages::SV) {
                matrix.app->sv++;
            }
        }

        //! Clean frame for the next cycle
        frame.num = 0;
    }
};

template< typename TMatrix >
struct IPStage 
{
    static void ApplyTo(TMatrix &matrix) {
        typename TMatrix::Frame &frame = matrix.stages[EStages::IP];

        for (unsigned i=0;i<frame.num;++i) {
            unsigned proto = *(unsigned *)frame.buf[i];
            if (proto == EStages::IP) {
                matrix.app->ip++;
            }
        }

        //! Clean frame for the next cycle
        frame.num = 0;
    }
};

struct TestApp
{
    unsigned total = 0;
    unsigned goose = 0;
    unsigned sv = 0;
    unsigned ip = 0;
};

TEST(Pipeline, BasicUsage)
{
    using TestMatrix = Pipeline::Matrix< EStages,
                                         Pipeline::Frame< uint8_t, 32 >,
                                         TestApp >;
    TestApp app;
    TestMatrix matrix(&app);

    const unsigned IP = 5, GOOSE = 7, SV = 13,
                   TOTAL = (IP + GOOSE + SV);
    unsigned packets[TOTAL] = { 0 };
    unsigned ipNum = 0, goNum = 0, svNum = 0;
    for (unsigned i=0;i<TOTAL;++i) {
        if (ipNum < IP) {
            packets[i] = EStages::IP;
            ++ipNum;
            continue;
        }
        if (goNum < GOOSE) {
            packets[i] = EStages::GOOSE;
            ++goNum;
            continue;
        }
        if (svNum < SV) {
            packets[i] = EStages::SV;
            ++svNum;
            continue;
        }
    }
    ASSERT_EQ(ipNum, IP);
    ASSERT_EQ(goNum, GOOSE);
    ASSERT_EQ(svNum, SV);

    // Fill START_STAGE
    for (unsigned i=0;i<TOTAL;++i) {
        matrix.stages[START_STAGE].buf[i] = (uint8_t *)&packets[i];
    }
    matrix.stages[START_STAGE].num = TOTAL;

    Pipeline::StaticChain<
        RouterStage< TestMatrix >,
        GooseStage< TestMatrix >,
        SampledValuesStage< TestMatrix >,
        IPStage< TestMatrix >
    >::run(matrix);

    ASSERT_EQ(app.total, TOTAL);
    ASSERT_EQ(app.goose, GOOSE);
    ASSERT_EQ(app.sv, SV);
    ASSERT_EQ(app.ip, IP);
}

TEST(Pipeline, BasicUsage2)
{
    struct Sum
    {
        static inline void ApplyTo(int *result, int a1, int a2)
        {
            *result += a1 + a2;
        }
    };
    struct Mul
    {
        static inline void ApplyTo(int *result, int a1, int a2)
        {
            *result += a1 * a2;
        }
    };

    int result = 0;

    Pipeline::StaticChain<
        Sum,
        Mul
    >::run(&result, 1, 2);

    ASSERT_EQ(result, 5);
}

