// QMI8658.h — Stub for IMU (not present on this board)
// Scenes that use IMU data will get zeros and fall back to touch/auto behavior

#ifndef QMI8658_H
#define QMI8658_H

#include <stdint.h>
#include <string.h>

static inline bool QMI8658_init(void) { return false; }

static inline void QMI8658_read_xyz(float acc[3], float gyro[3], unsigned int *ts) {
    memset(acc, 0, 3 * sizeof(float));
    memset(gyro, 0, 3 * sizeof(float));
    if (ts) *ts = 0;
}

#endif
