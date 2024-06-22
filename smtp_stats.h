#ifndef TP_PROTOS_SMTP_STATS_H
#define TP_PROTOS_SMTP_STATS_H

struct current_stats {
    int historic_connections, concurrent_connections, bytes_transferred;
};

#endif