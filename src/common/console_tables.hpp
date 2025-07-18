#pragma once

#include "goose_container.hpp"
#include "sv_container.hpp"
#include "dpdk_cpp/dpdk_cyclestat_class.hpp"

#include <iostream>
#include <format>
#include <list>

namespace Console
{
    class GooseSource
    {
    public:
        static void PrintCfgTableHeader() {
            std::cout << std::format(" {:<20} | {:<10} | {:<20} | {:<32} | {:<32} | {:<10} |\n",
                                     "MAC", "APPID", "GOID", "GOCB", "DataSet", "CRev")
                      << std::string(142, '-')
                      << std::endl;
        }

        static void PrintCfgTableRow(::GooseSource::ptr g) {
            std::cout << std::format(" {:<20} | {:<10} | {:<20} | {:<32} | {:<32} | {:<10} |",
                                     g->GetDMAC().toString(),
                                     g->GetAppID(),
                                     g->GetGOID(),
                                     g->GetGOCBRef(),
                                     g->GetDataSetRef(),
                                     g->GetCRev())
                      << std::endl;
        }

        static void PrintTableHeader() {
            std::cout << std::format("{:<20} | {:<10} | {:<20} | {:<10} | {:<10} | {:<10} |\n",
                                     "MAC", "APPID", "GOID", "StNum", "SqNum", "ErrSeqCnt")
                      << std::string(97, '-')
                      << std::endl;
        }

        static void PrintTableRow(::GooseSource::ptr g) {
            std::cout << std::format("{:<20} | {:<10} | {:<20} | {:<10} | {:<10} | {:<10} |\n",
                                     g->GetPassport().dmac.toString(),
                                     g->GetAppID(),
                                     g->GetGOID(),
                                     g->GetState().stNum,
                                     g->GetState().sqNum,
                                     g->GetErrSeqNum()
                         );
        }
    };

    class SVStreamSource
    {
    public:
        static void PrintCfgTableHeader() {
            std::cout << std::format(" {:<20} | {:<10} | {:<20} | {:<10} |\n",
                                     "MAC", "APPID", "SVID", "CRev")
                      << std::string(72, '-')
                      << std::endl;
        }

        static void PrintCfgTableRow(::SVStreamSource::ptr s) {
            std::cout << std::format(" {:<20} | {:<10} | {:<20} | {:<10} |",
                                     s->GetDMAC().toString(),
                                     s->GetAppID(),
                                     s->GetSVID(),
                                     s->GetCRev())
                      << std::endl;
        }

        static void PrintTableHeader() {
            std::cout << std::format("{:<20} | {:<10} | {:<20} | {:<10} | {:<10} |\n",
                                     "DMAC", "APPID", "SVID", "SmpCnt", "ErrSeqCnt")
                      << std::string(84, '-')
                      << std::endl;
        }

        static void PrintTableRow(::SVStreamSource::ptr s) {
            std::cout << std::format("{:<20} | {:<10} | {:<20} | {:<10} | {:<10} |\n",
                                     s->GetDMAC().toString(),
                                     s->GetAppID(),
                                     s->GetSVID(),
                                     s->GetSmpCnt(),
                                     s->GetErrSeqNum());
        }
    };

    class CyclicStat
    {
    public:
        static void PrintTableHeader(const std::list<std::string> &names = {}) {
            std::cout << std::format("{:<16} | {:<10} | {:10} | {:<10} | {:<10} |",
                                     "", "Min(us)", "Max(us)", "Load %", "Wait %");
            for (auto n : names) {
                std::cout << std::format(" {:<10} |", n);
            }
            std::cout << std::endl
                      << std::string(70 + names.size() * 13, '-')
                      << std::endl;
        }

        static std::ostream& PrintTableRow(const std::string &label, const DPDK::CyclicStat &st) {
            std::cout << std::format("{:<16} | {:<10} | {:<10} | {:<10.3f} | {:<10.3f} |",
                                     label,
                                     st.GetMinProcUS(), st.GetMaxProcUS(),
                                     st.GetLoadPerc(), st.GetWaitPerc());
            return std::cout;
        }
    };
}

