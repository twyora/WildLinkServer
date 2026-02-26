#pragma once
#ifndef __MAX30102_ALGORITHM_H__
#define __MAX30102_ALGORITHM_H__
#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus

#include <stdint.h>

    /**
     * \brief        Calculate the heart rate and SpO2 level
     * \par          Details
     *               By detecting  peaks of PPG cycle and corresponding AC/DC of red/infra-red signal, the ratio for the SPO2 is computed.
     *               Since this algorithm is aiming for Arm M0/M3. formaula for SPO2 did not achieve the accuracy due to register overflow.
     *               Thus, accurate SPO2 is precalculated and save longo uch_spo2_table[] per each ratio.
     *
     * \param[in]    *pun_ir_buffer           - IR sensor data buffer
     * \param[in]    n_ir_buffer_length      - IR sensor data buffer length
     * \param[in]    *pun_red_buffer          - Red sensor data buffer
     * \param[out]    *pn_spo2                - Calculated SpO2 value
     * \param[out]    *pch_spo2_valid         - 1 if the calculated SpO2 value is valid
     * \param[out]    *pn_heart_rate          - Calculated heart rate value
     * \param[out]    *pch_hr_valid           - 1 if the calculated heart rate value is valid
     *
     * \retval       None
     */
    void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);

    /**
     * \brief        Find peaks
     * \par          Details
     *               Find at most MAX_NUM peaks above MIN_HEIGHT separated by at least MIN_DISTANCE
     *
     * \retval       None
     */
    void maxim_find_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num);

    /**
     * \brief        Find peaks above n_min_height
     * \par          Details
     *               Find all peaks above MIN_HEIGHT
     *
     * \retval       None
     */
    void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height);

    /**
     * \brief        Remove peaks
     * \par          Details
     *               Remove peaks separated by less than MIN_DISTANCE
     *
     * \retval       None
     */
    void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance);

    /**
     * \brief        Sort array
     * \par          Details
     *               Sort array in ascending order (insertion sort algorithm)
     *
     * \retval       None
     */
    void maxim_sort_ascend(int32_t *pn_x, int32_t n_size);

    /**
     * \brief        Sort indices
     * \par          Details
     *               Sort indices according to descending order (insertion sort algorithm)
     *
     * \retval       None
     */
    void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size);

#ifdef __cplusplus
}
#endif //__cplusplus
#endif //__MAX30102_ALGORITHM_H__