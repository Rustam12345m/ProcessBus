#pragma once

namespace Pipeline
{
    template< typename MBUF, unsigned MAX_NUM = 32 >
    struct Frame
    {
        MBUF*       buf[MAX_NUM] = {};
        unsigned    num = 0;

        inline void PutBuffer(MBUF *ptr) {
            buf[num] = ptr;
            ++num;
        }
    };

    template< typename TNodeEnum, typename TFrame, typename TApp = void >
    struct Matrix
    {
        using Frame = TFrame;
        Matrix(TApp *ptr) : app(ptr) {}

        Frame stages[TNodeEnum::STAGE_NUM] = {};
        TApp *app = nullptr;
    };

    template< typename TMatrix, void (*Op)(TMatrix &) >
    struct Stage
    {
        static void ApplyTo(TMatrix &matrix) {
            Op(matrix);
        }
    };

    template< typename... TStages >
    struct StaticChain
    {
        template< typename... Args >
        static void run(Args&&... args)
        {
            (TStages::ApplyTo(std::forward< Args >(args) ...), ...);
        }
    };
};

