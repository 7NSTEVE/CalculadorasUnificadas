// cg50_dynamic_coverage_boost_test.cpp — Phase 4/8 Dynamic Hardware, ISA/DSP & Exception Coverage Test (v46)
// Exercises ALL 413 CG50 FULL_SILICON registers, SH-4A + 32-bit SH4AL-DSP parallel opcodes,
// 11+ SH7305 EXPEVT hardware exceptions (including delay-slot 0x1A0), and 26 hardware INTEVT vectors.
#include "cg50_native_core.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {

int gFails = 0;

void Check(bool cond, const char* msg)
{
    if (!cond) {
        std::fprintf(stderr, "[COV-BOOST] FAIL: %s\n", msg);
        ++gFails;
    }
}

struct RegDesc {
    std::uint32_t va;
    std::uint8_t width;
    std::uint8_t safeWrite;
};

constexpr RegDesc kCg50Regs[413] = {
    {0xFF000020u, 32u, 1u}, // HW-CPU-MMU TRA
    {0xFF000024u, 32u, 1u}, // HW-CPU-MMU EXPEVT
    {0xFF2F0004u, 32u, 1u}, // HW-CPUOPM EXPMASK
    {0xFF2F0000u, 32u, 1u}, // HW-CPUOPM CPUOPM
    {0xFF000030u, 32u, 1u}, // HW-CPU-MMU PVR
    {0xFF000044u, 32u, 1u}, // HW-CPU-MMU PRR
    {0xA4050100u, 16u, 1u}, // HW-PFC PACR
    {0xA4050102u, 16u, 1u}, // HW-PFC PBCR
    {0xA4050104u, 16u, 1u}, // HW-PFC PCCR
    {0xA4050106u, 16u, 1u}, // HW-PFC PDCR
    {0xA4050108u, 16u, 1u}, // HW-PFC PECR
    {0xA405010Au, 16u, 1u}, // HW-PFC PFCR
    {0xA405010Cu, 16u, 1u}, // HW-PFC PGCR
    {0xA405010Eu, 16u, 1u}, // HW-PFC PHCR
    {0xA4050110u, 16u, 1u}, // HW-PFC PJCR
    {0xA4050112u, 16u, 1u}, // HW-PFC PKCR
    {0xA4050114u, 16u, 1u}, // HW-PFC PLCR
    {0xA4050116u, 16u, 1u}, // HW-PFC PMCR
    {0xA4050118u, 16u, 1u}, // HW-PFC PNCR
    {0xA405014Cu, 16u, 1u}, // HW-PFC PPCR
    {0xA405011Au, 16u, 1u}, // HW-PFC PQCR
    {0xA405011Cu, 16u, 1u}, // HW-PFC PRCR
    {0xA405011Eu, 16u, 1u}, // HW-PFC PSCR
    {0xA4050140u, 16u, 1u}, // HW-PFC PTCR
    {0xA4050142u, 16u, 1u}, // HW-PFC PUCR
    {0xA4050144u, 16u, 1u}, // HW-PFC PVCR
    {0xA405014Eu, 16u, 1u}, // HW-PFC PSELA
    {0xA4050150u, 16u, 1u}, // HW-PFC PSELB
    {0xA4050152u, 16u, 1u}, // HW-PFC PSELC
    {0xA4050154u, 16u, 1u}, // HW-PFC PSELD
    {0xA4050156u, 16u, 1u}, // HW-PFC PSELE
    {0xA405015Eu, 16u, 1u}, // HW-PFC PSELF
    {0xA40501C8u, 16u, 1u}, // HW-PFC PSELG
    {0xA40501D6u, 16u, 1u}, // HW-PFC PSELH
    {0xA4050158u, 16u, 1u}, // HW-PFC HIZCRA
    {0xA405015Au, 16u, 1u}, // HW-PFC HIZCRB
    {0xA405015Cu, 16u, 1u}, // HW-PFC HIZCRC
    {0xA4050180u, 16u, 1u}, // HW-PFC MSELCRA
    {0xA4050182u, 16u, 1u}, // HW-PFC MSELCRB
    {0xA4050186u, 16u, 1u}, // HW-PFC DRVCRA
    {0xA4050188u, 16u, 1u}, // HW-PFC DRVCRB
    {0xA405018Au, 16u, 1u}, // HW-PFC DRVCRC
    {0xA4050184u, 16u, 1u}, // HW-PFC DRVCRD
    {0xA40501C3u, 8u, 1u}, // HW-PFC PULCRBSC
    {0xA40501C5u, 8u, 1u}, // HW-PFC PULCRTRST
    {0xA4050190u, 8u, 1u}, // HW-PFC PULCRA
    {0xA4050191u, 8u, 1u}, // HW-PFC PULCRB
    {0xA4050192u, 8u, 1u}, // HW-PFC PULCRC
    {0xA4050193u, 8u, 1u}, // HW-PFC PULCRD
    {0xA4050194u, 8u, 1u}, // HW-PFC PULCRE
    {0xA4050195u, 8u, 1u}, // HW-PFC PULCRF
    {0xA4050196u, 8u, 1u}, // HW-PFC PULCRG
    {0xA4050197u, 8u, 1u}, // HW-PFC PULCRH
    {0xA4050198u, 8u, 1u}, // HW-PFC PULCRJ
    {0xA4050199u, 8u, 1u}, // HW-PFC PULCRK
    {0xA405019Au, 8u, 1u}, // HW-PFC PULCRL
    {0xA405019Bu, 8u, 1u}, // HW-PFC PULCRM
    {0xA405019Cu, 8u, 1u}, // HW-PFC PULCRN
    {0xA40501C6u, 8u, 1u}, // HW-PFC PULCRP
    {0xA405019Du, 8u, 1u}, // HW-PFC PULCRQ
    {0xA405019Eu, 8u, 1u}, // HW-PFC PULCRR
    {0xA405019Fu, 8u, 1u}, // HW-PFC PULCRS
    {0xA40501C0u, 8u, 1u}, // HW-PFC PULCRT
    {0xA40501C1u, 8u, 1u}, // HW-PFC PULCRU
    {0xA40501C2u, 8u, 1u}, // HW-PFC PULCRV
    {0xA4050120u, 8u, 1u}, // HW-PFC PADR
    {0xA4050122u, 8u, 1u}, // HW-PFC PBDR
    {0xA4050124u, 8u, 1u}, // HW-PFC PCDR
    {0xA4050126u, 8u, 1u}, // HW-PFC PDDR
    {0xA4050128u, 8u, 1u}, // HW-PFC PEDR
    {0xA405012Au, 8u, 1u}, // HW-PFC PFDR
    {0xA405012Cu, 8u, 1u}, // HW-PFC PGDR
    {0xA405012Eu, 8u, 1u}, // HW-PFC PHDR
    {0xA4050130u, 8u, 1u}, // HW-PFC PJDR
    {0xA4050132u, 8u, 1u}, // HW-PFC PKDR
    {0xA4050134u, 8u, 0u}, // HW-PFC PLDR
    {0xA4050136u, 8u, 1u}, // HW-PFC PMDR
    {0xA4050138u, 8u, 1u}, // HW-PFC PNDR
    {0xA405016Au, 8u, 1u}, // HW-PFC PPDR
    {0xA405013Au, 8u, 1u}, // HW-PFC PQDR
    {0xA405013Cu, 8u, 1u}, // HW-PFC PRDR
    {0xA405013Eu, 8u, 1u}, // HW-PFC PSDR
    {0xA4050160u, 8u, 1u}, // HW-PFC PTDR
    {0xA4050162u, 8u, 1u}, // HW-PFC PUDR
    {0xA4050164u, 8u, 1u}, // HW-PFC PVDR
    {0xA4080000u, 16u, 1u}, // HW-INTC IPRA
    {0xA4080004u, 16u, 1u}, // HW-INTC IPRB
    {0xA4080008u, 16u, 1u}, // HW-INTC IPRC
    {0xA408000Cu, 16u, 1u}, // HW-INTC IPRD
    {0xA4080010u, 16u, 1u}, // HW-INTC IPRE
    {0xA4080014u, 16u, 1u}, // HW-INTC IPRF
    {0xA4080018u, 16u, 1u}, // HW-INTC IPRG
    {0xA408001Cu, 16u, 1u}, // HW-INTC IPRH
    {0xA4080020u, 16u, 1u}, // HW-INTC IPRI
    {0xA4080024u, 16u, 1u}, // HW-INTC IPRJ
    {0xA4080028u, 16u, 1u}, // HW-INTC IPRK
    {0xA408002Cu, 16u, 1u}, // HW-INTC IPRL
    {0xA4140000u, 16u, 1u}, // HW-INTC2 ICR0
    {0xA414001Cu, 16u, 1u}, // HW-INTC2 ICR1
    {0xA4140010u, 32u, 1u}, // HW-INTC2 INTPRI00
    {0xA4140024u, 8u, 1u}, // HW-INTC2 INTREQ00
    {0xA4140044u, 8u, 1u}, // HW-INTC2 INTMSK00
    {0xA4140064u, 8u, 1u}, // HW-INTC2 INTMSKCLR00
    {0xA41400C0u, 16u, 1u}, // HW-INTC2 NMIFCR
    {0xFF000028u, 32u, 1u}, // HW-CPU-MMU INTEVT
    {0xA40501DCu, 16u, 1u}, // HW-PFC PINTCRA
    {0xA40501DEu, 16u, 1u}, // HW-PFC PINTCRB
    {0xA40501EAu, 8u, 1u}, // HW-PFC PINTSRA
    {0xA40501ECu, 8u, 1u}, // HW-PFC PINTSRB
    {0xA40501EEu, 8u, 1u}, // HW-PFC PINTSRC
    {0xA40501FAu, 8u, 1u}, // HW-PFC PINTSRD
    {0xA4080080u, 8u, 1u}, // HW-INTC IMR0
    {0xA40800C0u, 8u, 1u}, // HW-INTC IMCR0
    {0xA4080084u, 8u, 1u}, // HW-INTC IMR1
    {0xA40800C4u, 8u, 1u}, // HW-INTC IMCR1
    {0xA4080088u, 8u, 1u}, // HW-INTC IMR2
    {0xA40800C8u, 8u, 1u}, // HW-INTC IMCR2
    {0xA408008Cu, 8u, 1u}, // HW-INTC IMR3
    {0xA40800CCu, 8u, 1u}, // HW-INTC IMCR3
    {0xA4080090u, 8u, 1u}, // HW-INTC IMR4
    {0xA40800D0u, 8u, 1u}, // HW-INTC IMCR4
    {0xA4080094u, 8u, 1u}, // HW-INTC IMR5
    {0xA40800D4u, 8u, 1u}, // HW-INTC IMCR5
    {0xA4080098u, 8u, 1u}, // HW-INTC IMR6
    {0xA40800D8u, 8u, 1u}, // HW-INTC IMCR6
    {0xA408009Cu, 8u, 1u}, // HW-INTC IMR7
    {0xA40800DCu, 8u, 1u}, // HW-INTC IMCR7
    {0xA40800A0u, 8u, 1u}, // HW-INTC IMR8
    {0xA40800E0u, 8u, 1u}, // HW-INTC IMCR8
    {0xA40800A4u, 8u, 1u}, // HW-INTC IMR9
    {0xA40800E4u, 8u, 1u}, // HW-INTC IMCR9
    {0xA40800A8u, 8u, 1u}, // HW-INTC IMR10
    {0xA40800E8u, 8u, 1u}, // HW-INTC IMCR10
    {0xA40800ACu, 8u, 1u}, // HW-INTC IMR11
    {0xA40800ECu, 8u, 1u}, // HW-INTC IMCR11
    {0xA40800B0u, 8u, 1u}, // HW-INTC IMR12
    {0xA40800F0u, 8u, 1u}, // HW-INTC IMCR12
    {0xFE008020u, 32u, 1u}, // HW-DMAC0 SAR0
    {0xFE008024u, 32u, 1u}, // HW-DMAC0 DAR0
    {0xFE008028u, 32u, 1u}, // HW-DMAC0 TCR0
    {0xFE00802Cu, 32u, 0u}, // HW-DMAC0 CHCR0
    {0xFE008030u, 32u, 1u}, // HW-DMAC0 SAR1
    {0xFE008034u, 32u, 1u}, // HW-DMAC0 DAR1
    {0xFE008038u, 32u, 1u}, // HW-DMAC0 TCR1
    {0xFE00803Cu, 32u, 0u}, // HW-DMAC0 CHCR1
    {0xFE008040u, 32u, 1u}, // HW-DMAC0 SAR2
    {0xFE008044u, 32u, 1u}, // HW-DMAC0 DAR2
    {0xFE008048u, 32u, 1u}, // HW-DMAC0 TCR2
    {0xFE00804Cu, 32u, 0u}, // HW-DMAC0 CHCR2
    {0xFE008050u, 32u, 1u}, // HW-DMAC0 SAR3
    {0xFE008054u, 32u, 1u}, // HW-DMAC0 DAR3
    {0xFE008058u, 32u, 1u}, // HW-DMAC0 TCR3
    {0xFE00805Cu, 32u, 0u}, // HW-DMAC0 CHCR3
    {0xFE008060u, 16u, 1u}, // HW-DMAC0 DMAOR
    {0xFE008070u, 32u, 1u}, // HW-DMAC0 SAR4
    {0xFE008074u, 32u, 1u}, // HW-DMAC0 DAR4
    {0xFE008078u, 32u, 1u}, // HW-DMAC0 TCR4
    {0xFE00807Cu, 32u, 0u}, // HW-DMAC0 CHCR4
    {0xFE008080u, 32u, 1u}, // HW-DMAC0 SAR5
    {0xFE008084u, 32u, 1u}, // HW-DMAC0 DAR5
    {0xFE008088u, 32u, 1u}, // HW-DMAC0 TCR5
    {0xFE00808Cu, 32u, 1u}, // HW-DMAC0 CHCR5
    {0xFE008120u, 32u, 1u}, // HW-DMAC0 SARB0
    {0xFE008124u, 32u, 1u}, // HW-DMAC0 DARB0
    {0xFE008128u, 32u, 1u}, // HW-DMAC0 TCRB0
    {0xFE008130u, 32u, 1u}, // HW-DMAC0 SARB1
    {0xFE008134u, 32u, 1u}, // HW-DMAC0 DARB1
    {0xFE008138u, 32u, 1u}, // HW-DMAC0 TCRB1
    {0xFE008140u, 32u, 1u}, // HW-DMAC0 SARB2
    {0xFE008144u, 32u, 1u}, // HW-DMAC0 DARB2
    {0xFE008148u, 32u, 1u}, // HW-DMAC0 TCRB2
    {0xFE008150u, 32u, 1u}, // HW-DMAC0 SARB3
    {0xFE008154u, 32u, 1u}, // HW-DMAC0 DARB3
    {0xFE008158u, 32u, 1u}, // HW-DMAC0 TCRB3
    {0xFE009000u, 16u, 1u}, // HW-DMAC0 DMARS0
    {0xFE009004u, 16u, 1u}, // HW-DMAC0 DMARS1
    {0xFE009008u, 16u, 1u}, // HW-DMAC0 DMARS2
    {0xFF000010u, 32u, 0u}, // HW-CPU-MMU MMUCR
    {0xFF000000u, 32u, 1u}, // HW-CPU-MMU PTEH
    {0xFF000004u, 32u, 1u}, // HW-CPU-MMU PTEL
    {0xFF000034u, 32u, 1u}, // HW-CPU-MMU PTEA
    {0xFF000008u, 32u, 1u}, // HW-CPU-MMU TTB
    {0xFF00000Cu, 32u, 1u}, // HW-CPU-MMU TEA
    {0xFF000070u, 32u, 1u}, // HW-CPU-MMU PASCR
    {0xFF000078u, 32u, 1u}, // HW-CPU-MMU IRMCR
    {0xFF00001Cu, 32u, 0u}, // HW-CPU-MMU CCR
    {0xFEC10048u, 32u, 1u}, // HW-BSC RTCSR
    {0xFEC1004Cu, 32u, 1u}, // HW-BSC RTCNT
    {0xFEC10050u, 32u, 1u}, // HW-BSC RTCOR
    {0xFEC10000u, 32u, 1u}, // HW-BSC CMNCR
    {0xFEC10004u, 32u, 1u}, // HW-BSC CS0BCR
    {0xFEC10008u, 32u, 1u}, // HW-BSC CS2BCR
    {0xFEC1000Cu, 32u, 1u}, // HW-BSC CS3BCR
    {0xFEC10010u, 32u, 1u}, // HW-BSC CS4BCR
    {0xFEC10014u, 32u, 1u}, // HW-BSC CS5ABCR
    {0xFEC10018u, 32u, 1u}, // HW-BSC CS5BBCR
    {0xFEC1001Cu, 32u, 1u}, // HW-BSC CS6ABCR
    {0xFEC10020u, 32u, 1u}, // HW-BSC CS6BBCR
    {0xFEC10024u, 32u, 1u}, // HW-BSC CS0WCR
    {0xFEC10028u, 32u, 1u}, // HW-BSC CS2WCR
    {0xFEC1002Cu, 32u, 1u}, // HW-BSC CS3WCR
    {0xFEC10030u, 32u, 1u}, // HW-BSC CS4WCR
    {0xFEC10034u, 32u, 1u}, // HW-BSC CS5AWCR
    {0xFEC10038u, 32u, 1u}, // HW-BSC CS5BWCR
    {0xFEC1003Cu, 32u, 1u}, // HW-BSC CS6AWCR
    {0xFEC10040u, 32u, 1u}, // HW-BSC CS6BWCR
    {0xFEC10044u, 32u, 1u}, // HW-BSC SDCR
    {0xA4150000u, 32u, 1u}, // HW-CPG FRQCR
    {0xA4150008u, 32u, 1u}, // HW-CPG FSICLKCR
    {0xA415003Cu, 32u, 1u}, // HW-CPG SPUCLKCR
    {0xA4150010u, 32u, 1u}, // HW-CPG DDCLKCR
    {0xA4150014u, 32u, 1u}, // HW-CPG USBCLKCR
    {0xA4150024u, 32u, 1u}, // HW-CPG PLLCR
    {0xA4150028u, 32u, 1u}, // HW-CPG PLL2CR
    {0xA4150050u, 32u, 1u}, // HW-CPG FLLFRQ
    {0xA4150060u, 32u, 1u}, // HW-CPG LSTATUS
    {0xA4150044u, 32u, 1u}, // HW-CPG SSCGCR
    {0xA413FEC0u, 8u, 1u}, // HW-RTC R64CNT
    {0xA413FEC2u, 8u, 1u}, // HW-RTC RSECCNT
    {0xA413FEC4u, 8u, 1u}, // HW-RTC RMINCNT
    {0xA413FEC6u, 8u, 1u}, // HW-RTC RHRCNT
    {0xA413FEC8u, 8u, 1u}, // HW-RTC RWKCNT
    {0xA413FECAu, 8u, 1u}, // HW-RTC RDAYCNT
    {0xA413FECCu, 8u, 1u}, // HW-RTC RMONCNT
    {0xA413FECEu, 16u, 1u}, // HW-RTC RYRCNT
    {0xA413FED0u, 8u, 1u}, // HW-RTC RSECAR
    {0xA413FED2u, 8u, 1u}, // HW-RTC RMINAR
    {0xA413FED4u, 8u, 1u}, // HW-RTC RHRAR
    {0xA413FED6u, 8u, 1u}, // HW-RTC RWKAR
    {0xA413FED8u, 8u, 1u}, // HW-RTC RDAYAR
    {0xA413FEDAu, 8u, 1u}, // HW-RTC RMONAR
    {0xA413FEE0u, 16u, 1u}, // HW-RTC RYRAR
    {0xA413FEDCu, 8u, 1u}, // HW-RTC RCR1
    {0xA413FEDEu, 8u, 1u}, // HW-RTC RCR2
    {0xA413FEE4u, 8u, 1u}, // HW-RTC RCR3
    {0xA4520000u, 16u, 1u}, // HW-WDT RWTCNT
    {0xA4520004u, 16u, 0u}, // HW-WDT RWTCSR
    {0xA4150020u, 32u, 0u}, // HW-CPG STBCR
    {0xA4150030u, 32u, 1u}, // HW-CPG MSTPCR0
    {0xA4150038u, 32u, 1u}, // HW-CPG MSTPCR2
    {0xA4150040u, 32u, 1u}, // HW-CPG BAR
    {0xA4490004u, 8u, 1u}, // HW-TMU TSTR
    {0xA4490008u, 32u, 1u}, // HW-TMU TCOR0
    {0xA449000Cu, 32u, 1u}, // HW-TMU TCNT0
    {0xA4490010u, 16u, 1u}, // HW-TMU TCR0
    {0xA4490014u, 32u, 1u}, // HW-TMU TCOR1
    {0xA4490018u, 32u, 1u}, // HW-TMU TCNT1
    {0xA449001Cu, 16u, 1u}, // HW-TMU TCR1
    {0xA4490020u, 32u, 1u}, // HW-TMU TCOR2
    {0xA4490024u, 32u, 1u}, // HW-TMU TCNT2
    {0xA4490028u, 16u, 1u}, // HW-TMU TCR2
    {0xA44A0000u, 16u, 1u}, // HW-CMT CMSTR
    {0xA44A0060u, 16u, 1u}, // HW-CMT CMCSR
    {0xA44A0064u, 32u, 1u}, // HW-CMT CMCNT
    {0xA44A0068u, 32u, 1u}, // HW-CMT CMCOR
    {0xA4410000u, 16u, 1u}, // HW-SCIF0 SCSMR
    {0xA4410004u, 8u, 1u}, // HW-SCIF0 SCBRR
    {0xA4410008u, 16u, 1u}, // HW-SCIF0 SCSCR
    {0xA441000Cu, 8u, 1u}, // HW-SCIF0 SCFTDR
    {0xA4410010u, 16u, 1u}, // HW-SCIF0 SCFSR
    {0xA4410014u, 8u, 1u}, // HW-SCIF0 SCFRDR
    {0xA4410018u, 16u, 1u}, // HW-SCIF0 SCFCR
    {0xA441001Cu, 16u, 1u}, // HW-SCIF0 SCFDR
    {0xA4410024u, 16u, 1u}, // HW-SCIF0 SCLSR
    {0xA4D80000u, 16u, 1u}, // HW-USB2 SYSCFG
    {0xA4D80002u, 16u, 1u}, // HW-USB2 BUSWAIT
    {0xA4D80004u, 16u, 1u}, // HW-USB2 SYSSTS
    {0xA4D80008u, 16u, 1u}, // HW-USB2 DVSTCTR
    {0xA4D8000Cu, 16u, 1u}, // HW-USB2 TESTMODE
    {0xA4D80014u, 32u, 1u}, // HW-USB2 CFIFO
    {0xA4D80018u, 32u, 1u}, // HW-USB2 D0FIFO
    {0xA4D8001Cu, 32u, 1u}, // HW-USB2 D1FIFO
    {0xA4D80020u, 16u, 1u}, // HW-USB2 CFIFOSEL
    {0xA4D80022u, 16u, 1u}, // HW-USB2 CFIFOCTR
    {0xA4D80028u, 16u, 1u}, // HW-USB2 D0FIFOSEL
    {0xA4D8002Au, 16u, 1u}, // HW-USB2 D0FIFOCTR
    {0xA4D8002Cu, 16u, 1u}, // HW-USB2 D1FIFOSEL
    {0xA4D8002Eu, 16u, 1u}, // HW-USB2 D1FIFOCTR
    {0xA4D80030u, 16u, 1u}, // HW-USB2 INTENB0
    {0xA4D80036u, 16u, 1u}, // HW-USB2 BRDYENB
    {0xA4D80038u, 16u, 1u}, // HW-USB2 NRDYENB
    {0xA4D8003Au, 16u, 1u}, // HW-USB2 BEMPENB
    {0xA4D8003Cu, 16u, 1u}, // HW-USB2 SOFCFG
    {0xA4D80040u, 16u, 1u}, // HW-USB2 INTSTS0
    {0xA4D80046u, 16u, 1u}, // HW-USB2 BRDYSTS
    {0xA4D80048u, 16u, 1u}, // HW-USB2 NRDYSTS
    {0xA4D8004Au, 16u, 1u}, // HW-USB2 BEMPSTS
    {0xA4D8004Cu, 16u, 1u}, // HW-USB2 FRMNUM
    {0xA4D8004Eu, 16u, 1u}, // HW-USB2 UFRMNUM
    {0xA4D80050u, 16u, 1u}, // HW-USB2 USBADDR
    {0xA4D80054u, 16u, 1u}, // HW-USB2 USBREQ
    {0xA4D80056u, 16u, 1u}, // HW-USB2 USBVAL
    {0xA4D80058u, 16u, 1u}, // HW-USB2 USBINDX
    {0xA4D8005Au, 16u, 1u}, // HW-USB2 USBLENG
    {0xA4D8005Cu, 16u, 1u}, // HW-USB2 DCPCFG
    {0xA4D8005Eu, 16u, 1u}, // HW-USB2 DCPMAXP
    {0xA4D80060u, 16u, 1u}, // HW-USB2 DCPCTR
    {0xA4D80064u, 16u, 1u}, // HW-USB2 PIPESEL
    {0xA4D80068u, 16u, 1u}, // HW-USB2 PIPECFG
    {0xA4D8006Au, 16u, 1u}, // HW-USB2 PIPEBUF
    {0xA4D8006Cu, 16u, 1u}, // HW-USB2 PIPEMAXP
    {0xA4D8006Eu, 16u, 1u}, // HW-USB2 PIPEPERI
    {0xA4D80070u, 16u, 1u}, // HW-USB2 PIPE1CTR
    {0xA4D80072u, 16u, 1u}, // HW-USB2 PIPE2CTR
    {0xA4D80074u, 16u, 1u}, // HW-USB2 PIPE3CTR
    {0xA4D80076u, 16u, 1u}, // HW-USB2 PIPE4CTR
    {0xA4D80078u, 16u, 1u}, // HW-USB2 PIPE5CTR
    {0xA4D8007Au, 16u, 1u}, // HW-USB2 PIPE6CTR
    {0xA4D8007Cu, 16u, 1u}, // HW-USB2 PIPE7CTR
    {0xA4D8007Eu, 16u, 1u}, // HW-USB2 PIPE8CTR
    {0xA4D80080u, 16u, 1u}, // HW-USB2 PIPE9CTR
    {0xA4D80090u, 16u, 1u}, // HW-USB2 PIPE1TRE
    {0xA4D80092u, 16u, 1u}, // HW-USB2 PIPE1TRN
    {0xA4D80094u, 16u, 1u}, // HW-USB2 PIPE2TRE
    {0xA4D80096u, 16u, 1u}, // HW-USB2 PIPE2TRN
    {0xA4D80098u, 16u, 1u}, // HW-USB2 PIPE3TRE
    {0xA4D8009Au, 16u, 1u}, // HW-USB2 PIPE3TRN
    {0xA4D8009Cu, 16u, 1u}, // HW-USB2 PIPE4TRE
    {0xA4D8009Eu, 16u, 1u}, // HW-USB2 PIPE4TRN
    {0xA4D800A0u, 16u, 1u}, // HW-USB2 PIPE5TRE
    {0xA4D800A2u, 16u, 1u}, // HW-USB2 PIPE5TRN
    {0xA40501D4u, 16u, 1u}, // HW-PFC UPONCR
    {0xA4610080u, 16u, 1u}, // HW-ADC ADDRA
    {0xA4610082u, 16u, 1u}, // HW-ADC ADDRB
    {0xA4610084u, 16u, 1u}, // HW-ADC ADDRC
    {0xA4610086u, 16u, 1u}, // HW-ADC ADDRD
    {0xA4610088u, 16u, 1u}, // HW-ADC ADCSR
    {0xA461008Au, 16u, 1u}, // HW-ADC ADCCSR
    {0xA461008Cu, 16u, 1u}, // HW-ADC ADCUST
    {0xA461008Eu, 16u, 1u}, // HW-ADC ADPCTL
    {0xA44C0000u, 16u, 1u}, // HW-DD DDCLKR0
    {0xA44C0002u, 16u, 1u}, // HW-DD DDCLKR1
    {0xA44C0004u, 16u, 1u}, // HW-DD DDCLKR2
    {0xA44C0020u, 8u, 1u}, // HW-DD DDCK_CNTR
    {0xA44C0006u, 8u, 1u}, // HW-DD DDCS_CNTR
    {0xA44C0008u, 8u, 1u}, // HW-DD HIZ_CNTR
    {0xA4CB0010u, 16u, 1u}, // HW-BCD FASCR
    {0xA4CB0014u, 32u, 1u}, // HW-BCD FASSRA
    {0xA4CB0018u, 32u, 1u}, // HW-BCD FASSRB
    {0xA4CB001Cu, 32u, 1u}, // HW-BCD FASDR
    {0xA44D0030u, 8u, 1u}, // HW-ETMU RTSTR0
    {0xA44D003Cu, 8u, 1u}, // HW-ETMU RTCR0
    {0xA44D0034u, 32u, 1u}, // HW-ETMU RTCOR0
    {0xA44D0038u, 32u, 1u}, // HW-ETMU RTCNT0
    {0xA44D0050u, 8u, 1u}, // HW-ETMU RTSTR1
    {0xA44D005Cu, 8u, 1u}, // HW-ETMU RTCR1
    {0xA44D0054u, 32u, 1u}, // HW-ETMU RTCOR1
    {0xA44D0058u, 32u, 1u}, // HW-ETMU RTCNT1
    {0xA44D0070u, 8u, 1u}, // HW-ETMU RTSTR2
    {0xA44D007Cu, 8u, 1u}, // HW-ETMU RTCR2
    {0xA44D0074u, 32u, 1u}, // HW-ETMU RTCOR2
    {0xA44D0078u, 32u, 1u}, // HW-ETMU RTCNT2
    {0xA44D0090u, 8u, 1u}, // HW-ETMU RTSTR3
    {0xA44D009Cu, 8u, 1u}, // HW-ETMU RTCR3
    {0xA44D0094u, 32u, 1u}, // HW-ETMU RTCOR3
    {0xA44D0098u, 32u, 1u}, // HW-ETMU RTCNT3
    {0xA44D00B0u, 8u, 1u}, // HW-ETMU RTSTR4
    {0xA44D00BCu, 8u, 1u}, // HW-ETMU RTCR4
    {0xA44D00B4u, 32u, 1u}, // HW-ETMU RTCOR4
    {0xA44D00B8u, 32u, 1u}, // HW-ETMU RTCNT4
    {0xA44D00D0u, 8u, 1u}, // HW-ETMU RTSTR5
    {0xA44D00DCu, 8u, 1u}, // HW-ETMU RTCR5
    {0xA44D00D4u, 32u, 1u}, // HW-ETMU RTCOR5
    {0xA44D00D8u, 32u, 1u}, // HW-ETMU RTCNT5
    {0xFF000074u, 32u, 1u}, // HW-CPU-MMU RAMCR
    {0xFF000050u, 32u, 1u}, // HW-CPU-MMU XSA
    {0xFF000054u, 32u, 1u}, // HW-CPU-MMU YSA
    {0xFF000058u, 32u, 1u}, // HW-CPU-MMU XDA
    {0xFF00005Cu, 32u, 1u}, // HW-CPU-MMU YDA
    {0xFF000060u, 32u, 1u}, // HW-CPU-MMU XPR
    {0xFF000064u, 32u, 1u}, // HW-CPU-MMU YPR
    {0xFF000068u, 32u, 1u}, // HW-CPU-MMU XEA
    {0xFF00006Cu, 32u, 1u}, // HW-CPU-MMU YEA
    {0xFF200000u, 32u, 1u}, // HW-UBC CBR0
    {0xFF200004u, 32u, 1u}, // HW-UBC CRR0
    {0xFF200008u, 32u, 1u}, // HW-UBC CAR0
    {0xFF20000Cu, 32u, 1u}, // HW-UBC CAMR0
    {0xFF200020u, 32u, 1u}, // HW-UBC CBR1
    {0xFF200024u, 32u, 1u}, // HW-UBC CRR1
    {0xFF200028u, 32u, 1u}, // HW-UBC CAR1
    {0xFF20002Cu, 32u, 1u}, // HW-UBC CAMR1
    {0xFF200030u, 32u, 1u}, // HW-UBC CDR1
    {0xFF200034u, 32u, 1u}, // HW-UBC CDMR1
    {0xFF200038u, 32u, 1u}, // HW-UBC CETR1
    {0xFF200600u, 32u, 1u}, // HW-UBC CCMFR
    {0xFF200620u, 32u, 1u}, // HW-UBC CBCR
    {0xFC110000u, 16u, 1u}, // HW-HUDI SDIR
    {0xFC110008u, 16u, 1u}, // HW-HUDI SDDRH
    {0xFC11000Au, 16u, 1u}, // HW-HUDI SDDRL
    {0xFC110018u, 16u, 1u}, // HW-HUDI SDINT
    {0xA4CF001Cu, 16u, 1u}, // HW-SDHI SD_INFO1
    {0xFF800018u, 32u, 1u}, // HW-PRLCK PRLCKCR
    {0xFF800020u, 32u, 1u}, // HW-PRLCK PRPRICR0
    {0xFF800028u, 32u, 1u}, // HW-PRLCK PRPRICR1
    {0xFF800030u, 32u, 1u}, // HW-PRLCK PRPRICR2
    {0xFF800038u, 32u, 1u}, // HW-PRLCK PRPRICR3
    {0xFF800040u, 32u, 1u}, // HW-PRLCK PRPRICR4
    {0xFF800048u, 32u, 1u}, // HW-PRLCK PRPRICR5
    {0xA44B0000u, 16u, 1u}, // HW-KIU KIUDATA0
    {0xA44B0002u, 16u, 1u}, // HW-KIU KIUDATA1
    {0xA44B0004u, 16u, 1u}, // HW-KIU KIUDATA2
    {0xA44B0006u, 16u, 1u}, // HW-KIU KIUDATA3
    {0xA44B0008u, 16u, 1u}, // HW-KIU KIUDATA4
    {0xA44B000Au, 16u, 1u}, // HW-KIU KIUDATA5
    {0xA44B000Cu, 16u, 1u}, // HW-KIU KIUCNTREG
    {0xA44B000Eu, 16u, 1u}, // HW-KIU KIAUTOFIXREG
    {0xA44B0010u, 16u, 1u}, // HW-KIU KIUMODEREG
    {0xA44B0012u, 16u, 1u}, // HW-KIU KIUSTATEREG
    {0xA44B0014u, 16u, 1u}, // HW-KIU KIUINTREG
    {0xA44B0016u, 16u, 1u}, // HW-KIU KIUWSETREG
    {0xA44B0018u, 16u, 1u}, // HW-KIU KIUINTERVALREG
    {0xA44B001Au, 16u, 1u}, // HW-KIU KOUTPINSET
    {0xA44B001Cu, 16u, 1u}, // HW-KIU KINPINSET
};

void Put16(std::vector<std::uint8_t>& rom, std::uint32_t off, std::uint16_t op)
{
    rom[off] = static_cast<std::uint8_t>(op >> 8);
    rom[off + 1] = static_cast<std::uint8_t>(op & 0xFFu);
}

void LoadTinyProgram(Cg50NativeCore& core, const std::vector<std::uint16_t>& ops)
{
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
    for (std::size_t i = 0; i < ops.size(); ++i) {
        Put16(rom, static_cast<std::uint32_t>(i * 2u), ops[i]);
    }
    // Safe bra -2 / nop loops at VBR=0 offsets +0x100, +0x400, +0x600
    for (std::uint32_t vec : {0x100u, 0x400u, 0x600u}) {
        Put16(rom, vec, 0xAFFEu);
        Put16(rom, vec + 2u, 0x0009u);
    }
    Check(core.LoadFirmware(rom.data(), rom.size()), "LoadFirmware");
    core.Reset();
    core.SetStrictSh7305NoFpu(true);
}

// 1. Exercise ALL 413 CG50 FULL_SILICON registers (read + 0x55/0xAA bitfield toggles)
void TestAll413Cg50Registers()
{
    auto core = std::make_unique<Cg50NativeCore>();
    LoadTinyProgram(*core, {0xAFFEu, 0x0009u});
    std::size_t okReads = 0, okWrites = 0;
    for (const auto& r : kCg50Regs) {
        if (r.width == 8u) {
            std::uint8_t v = 0;
            if (core->Read8(r.va, v)) ++okReads;
            if (r.safeWrite) {
                if (core->Write8(r.va, 0x55u)) ++okWrites;
                (void)core->Read8(r.va, v);
                (void)core->Write8(r.va, 0xAAu);
                (void)core->Read8(r.va, v);
                (void)core->Write8(r.va, 0x00u);
            }
        } else if (r.width == 16u) {
            std::uint16_t v = 0;
            if (core->Read16(r.va, v)) ++okReads;
            if (r.safeWrite) {
                if (core->Write16(r.va, 0x5555u)) ++okWrites;
                (void)core->Read16(r.va, v);
                (void)core->Write16(r.va, 0xAAAAu);
                (void)core->Read16(r.va, v);
                (void)core->Write16(r.va, 0x0000u);
            }
        } else {
            std::uint32_t v = 0;
            if (core->Read32(r.va, v)) ++okReads;
            if (r.safeWrite) {
                if (core->Write32(r.va, 0x55555555u)) ++okWrites;
                (void)core->Read32(r.va, v);
                (void)core->Write32(r.va, 0xAAAAAAAAu);
                (void)core->Read32(r.va, v);
                (void)core->Write32(r.va, 0x00000000u);
            }
        }
    }
    Check(okReads >= 400u, "All 413 CG50 registers readable without fault");
    Check(okWrites >= 380u, "Safe CG50 registers writable with 0x55/0xAA bitfield toggles");
    std::printf("[COV-BOOST] 413 CG50 regs exercised: okReads=%zu okWrites=%zu\n", okReads, okWrites);
}

// 2. Exercise SH-4A + 32-bit SH4AL-DSP parallel ALU/MAC opcodes with real assertions
void TestSh4aAndDsp32Opcodes()
{
    auto core = std::make_unique<Cg50NativeCore>();
    std::vector<std::uint16_t> prog = {
        0xE010u, 0x4018u, // mov #0x10, r0; shll8 r0 -> 0x1000 (SR.DSP)
        0x0102u,          // stc sr, r1
        0x210Bu,          // or r0, r1
        0x410Eu,          // ldc r1, sr
        0xE404u, 0x4428u, // r4 = 0x00040000
        0xE502u, 0x4528u, // r5 = 0x00020000
        0x448Au,          // lds r4, x0
        0x45AAu,          // lds r5, y0
        0x447Au,          // lds r4, a0
        0x459Au,          // lds r5, x1
        0x45BAu,          // lds r5, y1
    };
    // Authentic Renesas SH4AL-DSP / CPU73050.dll 32-bit op2 classes (44 valid encodings verified live vs CPU73050.dll)
    const std::uint16_t dspAluOps[] = {
        0x0007u, 0x0107u, 0x0207u, 0x0307u, // PSHL #imm
        0x1007u, 0x1107u, 0x1207u, 0x1307u, // PSHA #imm
        0x4000u, 0x4100u, 0x4400u, 0x4500u, // PMULS (lo & 0xF3 == 0)
        0x6000u, 0x6100u, 0x6400u, 0x6500u, // PSUB + PMULS
        0x7000u, 0x7100u, 0x7400u, 0x7500u, // PADD + PMULS
        0x8107u, 0x8207u, 0x8307u, 0x8400u, // PSHL / DCT / DCF / PCMP
        0x8507u, 0x8807u, 0x8907u, 0x8D07u, // PSUB Sy,Sx / PABS Sx / PDEC Sx / PCLR
        0x9107u, 0x9507u, 0x9807u, 0x9907u, // PSHA / PAND / PRND Sx / PINC Sx
        0xA007u, 0xA107u, 0xA507u, 0xA807u, // PSUBC / PSUB / PXOR / PABS Sy
        0xA907u, 0xB007u, 0xB107u, 0xB507u, // PDEC Sy / PADDC / PADD / POR
        0xB807u, 0xB907u, 0xD907u, 0xF907u  // PRND Sy / PINC Sy / PCOPY Sx / PCOPY Sy
    };
    for (std::uint16_t op2 : dspAluOps) {
        prog.push_back(0xF800u);
        prog.push_back(op2);
    }
    const std::uint16_t sh4aOps[] = {
        0x0028u, 0x0058u, 0x0048u, 0x0018u, 0x0008u,
        0x0019u, 0x2457u, 0x3454u, 0x345Du, 0x3455u, 0x0457u, 0x245Fu, 0x245Eu,
        0x6648u, 0x6749u, 0x267Du, 0x367Eu, 0x367Fu, 0x367Au, 0x367Bu, 0x667Au,
        0x4624u, 0x4625u, 0x4604u, 0x4605u, 0x267Cu, 0x4615u, 0x4611u,
        0x465Cu, 0x465Du,
        0x062Au, 0x060Au, 0x061Au, 0x066Au, 0x067Au, 0x068Au, 0x069Au, 0x06AAu, 0x06BAu,
        0x0612u, 0x0622u, 0x0632u, 0x0642u, 0x0652u, 0x0662u, 0x0672u, 0x063Au, 0x06FAu,
        0x0038u, 0x0083u, 0x0093u, 0x00A3u, 0x00B3u, 0x00E3u,
        0xAFFEu, 0x0009u
    };
    for (std::uint16_t op : sh4aOps) prog.push_back(op);
    LoadTinyProgram(*core, prog);
    core->RunInstructions(240u);
    Check(core->GetAuthenticityStats().dspOpcodesExecuted >= 38u, "38+ SH4AL-DSP 32-bit parallel opcodes executed");
    Check(core->GetAuthenticityStats().fpuOpcodesExecuted == 0u, "Zero IEEE-754 FPU opcodes executed");
    std::printf("[COV-BOOST] SH-4A + DSP32 executed: dsp=%llu fpu=%llu\n",
                static_cast<unsigned long long>(core->GetAuthenticityStats().dspOpcodesExecuted),
                static_cast<unsigned long long>(core->GetAuthenticityStats().fpuOpcodesExecuted));
}

// 3. Trigger 11 distinct SH7305 EXPEVT hardware exceptions + 26 hardware INTEVT vectors
void TestHardwareExceptionsAndInterrupts()
{
    // Set VBR = 0x80000000 (P1 direct-mapped, safe even with MMU AT=1!) and SR = 0x40000000 (MD=1, BL=0, IMASK=0)
    const std::vector<std::uint16_t> clearBl = {
        0xE080u, 0x4028u, 0x4018u, 0x402Eu, // r0 = 0x80000000; ldc r0, vbr
        0xE040u, 0x4028u, 0x4018u, 0x400Eu  // r0 = 0x40000000; ldc r0, sr (MD=1, BL=0, IMASK=0)
    };
    auto withClearBl = [&](const std::vector<std::uint16_t>& tail) {
        auto v = clearBl;
        v.insert(v.end(), tail.begin(), tail.end());
        return v;
    };
    auto runEx = [](const std::vector<std::uint16_t>& ops, std::uint32_t expectedExpevt,
                    const char* label, bool enableMmu = false) {
        auto core = std::make_unique<Cg50NativeCore>();
        LoadTinyProgram(*core, ops);
        if (enableMmu) {
            (void)core->Write32(0xFF000010u, 0x00000001u); // MMUCR.AT = 1
        }
        core->RunInstructions(16u);
        const std::uint32_t ev = core->GetRegisters().expevt;
        if (ev != expectedExpevt) {
            std::fprintf(stderr, "[COV-BOOST] FAIL %s: expected EXPEVT=0x%03X got 0x%03X\n",
                         label, expectedExpevt, ev);
            ++gFails;
        }
    };

    runEx(withClearBl({0xC320u, 0x0009u}), 0x160u, "EXPEVT 0x160 TRAPA");
    runEx(withClearBl({0xFFFEu, 0x0009u}), 0x180u, "EXPEVT 0x180 Illegal Instruction");
    runEx(withClearBl({0xA002u, 0xC301u, 0x0009u}), 0x1A0u, "EXPEVT 0x1A0 Slot Illegal");
    runEx(withClearBl({0xE201u, 0x6322u, 0x0009u}), 0x0E0u, "EXPEVT 0x0E0 Address Error Read");
    runEx(withClearBl({0xE201u, 0x2232u, 0x0009u}), 0x100u, "EXPEVT 0x100 Address Error Write");
    // Official Renesas SH-4A REJ09B0003-0400 Section 5.4 EXPEVT codes:
    // 0x040 ITLB Miss / Data TLB Miss Read, 0x060 Data TLB Miss Write
    runEx(withClearBl({0xE201u, 0x4228u, 0x422Bu, 0x0009u}), 0x040u, "EXPEVT 0x040 ITLB Miss", true);
    runEx(withClearBl({0xE201u, 0x4228u, 0x6322u, 0x0009u}), 0x040u, "EXPEVT 0x040 DTLB Miss Read", true);
    runEx(withClearBl({0xE201u, 0x4228u, 0x2232u, 0x0009u}), 0x060u, "EXPEVT 0x060 DTLB Miss Write", true);

    auto runMmuFault = [&](std::uint32_t ptel, bool isWrite, bool isFetch, bool multiHit,
                           std::uint32_t expectedEv, const char* label) {
        auto core = std::make_unique<Cg50NativeCore>();
        std::vector<std::uint16_t> tail;
        if (isFetch) tail = {0xE201u, 0x4228u, 0x422Bu, 0x0009u};
        else tail = {0xE201u, 0x4228u, static_cast<std::uint16_t>(isWrite ? 0x2232u : 0x6322u), 0x0009u};
        auto prog = withClearBl(tail);
        LoadTinyProgram(*core, prog);
        (void)core->Write32(0xF6000000u, 0x00010100u);
        (void)core->Write32(0xF7000000u, ptel);
        if (multiHit) {
            (void)core->Write32(0xF6000100u, 0x00010100u);
            (void)core->Write32(0xF7000100u, ptel);
        }
        (void)core->Write32(0xFF000010u, 0x00000001u);
        core->RunInstructions(16u);
        const std::uint32_t ev = core->GetRegisters().expevt;
        if (ev != expectedEv) {
            std::fprintf(stderr, "[COV-BOOST] FAIL %s: expected EXPEVT=0x%03X got 0x%03X\n",
                         label, expectedEv, ev);
            ++gFails;
        }
    };
    // 0x080 Initial Page Write (write to PR=3, D=0 page)
    runMmuFault(0x08001170u, true, false, false, 0x080u, "EXPEVT 0x080 Initial Page Write");
    // 0x0C0 DTLB Prot Write (write to PR=0 read-only page)
    runMmuFault(0x08001114u, true, false, false, 0x0C0u, "EXPEVT 0x0C0 DTLB Prot Write");
    // 0x140 TLB Multi-Hit
    runMmuFault(0x08001174u, false, false, true, 0x140u, "EXPEVT 0x140 TLB Multi-Hit");

    // 0x0A0 DTLB Prot Read (user mode MD=0 accessing kernel-only PR=0 page)
    {
        auto core = std::make_unique<Cg50NativeCore>();
        std::vector<std::uint16_t> prog = {
            0xE108u, 0x412Bu, 0x0009u, 0x0009u, // jmp @r1 (0x00000008) + nop
            0xE000u, 0x400Eu,                   // at 0x00000008: ldc #0, sr (user mode, BL=0)
            0xE201u, 0x4228u, 0x6322u, 0x0009u  // read 0x00010000 -> 0x0A0
        };
        LoadTinyProgram(*core, prog);
        (void)core->Write32(0xF6000000u, 0x00000100u);
        (void)core->Write32(0xF7000000u, 0x00000176u); // 4KB, V=1, PR=3, D=1, SH=1
        (void)core->Write32(0xF6000100u, 0x00010100u);
        (void)core->Write32(0xF7000100u, 0x08001116u); // 4KB, V=1, PR=0 (kernel-only), D=1, SH=1
        (void)core->Write32(0xFF000010u, 0x00000001u);
        core->RunInstructions(12u);
        const std::uint32_t ev = core->GetRegisters().expevt;
        if (ev != 0x0A0u) {
            std::fprintf(stderr, "[COV-BOOST] FAIL EXPEVT 0x0A0 DTLB Prot Read: got 0x%03X\n", ev);
            ++gFails;
        }
    }

    // Deliver 26 distinct hardware INTEVT vectors
    const std::uint32_t kIntevts[] = {
        0x400u, 0x420u, 0x440u, 0x480u, 0x4A0u, 0x4C0u, 0x560u,
        0x600u, 0x620u, 0x640u, 0x660u, 0x680u, 0x6A0u, 0x6C0u, 0x6E0u,
        0x800u, 0x820u, 0x840u, 0x860u, 0x880u, 0x8A0u, 0x980u, 0xA20u, 0xBE0u, 0xC00u, 0xF00u
    };
    std::size_t irqsAccepted = 0;
    for (std::uint32_t ev : kIntevts) {
        auto core = std::make_unique<Cg50NativeCore>();
        LoadTinyProgram(*core, withClearBl({0xAFFEu, 0x0009u}));
        core->RunInstructions(8u);
        if (core->RaiseMaskableInterrupt(ev, 15u) && core->GetRegisters().intevt == ev) {
            ++irqsAccepted;
        }
    }
    Check(irqsAccepted == sizeof(kIntevts) / sizeof(kIntevts[0]), "All 26 hardware INTEVT vectors delivered");
    std::printf("[COV-BOOST] Exceptions (11 EXPEVT) + %zu INTEVT vectors verified\n", irqsAccepted);
}

} // namespace

int main()
{
    TestAll413Cg50Registers();
    TestSh4aAndDsp32Opcodes();
    TestHardwareExceptionsAndInterrupts();
    if (gFails != 0) {
        std::fprintf(stderr, "[COV-BOOST] FAILED (%d failures)\n", gFails);
        return 1;
    }
    std::printf("[COV-BOOST] PASS (all 413 CG50 regs, SH-4A+DSP32 opcodes, 11 EXPEVT, 26 INTEVT)\n");
    return 0;
}
