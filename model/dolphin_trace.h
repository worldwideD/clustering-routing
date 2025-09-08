#ifndef DOLPHIN_TRACE_H
#define DOLPHIN_TRACE_H

enum class TracePacketType
{
    TASK = 1,
    ACK,
    DATA,
    NODEDOWEROFF,
    ND
};

enum class TraceAction
{
    RXFromUP = 1,
    RXFromDown,
    TXToUP,
    TXToDown,
    OTHERS
};

#endif