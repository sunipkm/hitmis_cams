/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/* picam_em_calibration.h - Teledyne Princeton Instruments EM Calibration API */
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
#if !defined PICAM_EM_CALIBRATION_H
#define PICAM_EM_CALIBRATION_H

#include "picam.h"

/******************************************************************************/
/* C++ Prologue                                                               */
/******************************************************************************/
#if defined __cplusplus && !defined PICAM_EXPORTS
    extern "C"
    {
#endif

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/* EM Calibration Access                                                      */
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_OpenCalibration(
    const PicamCameraID* id,
    PicamHandle*         calibration );
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_CloseCalibration( PicamHandle calibration );
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_GetOpenCalibrations(
    const PicamHandle** calibrations_array,
    piint*              calibrations_count ); /* ALLOCATES */
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_GetCameraID(
    PicamHandle    calibration,
    PicamCameraID* id );
/*----------------------------------------------------------------------------*/

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/* EM Calibration Parameter Values and Constraints                            */
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* EM Calibration Parameter Values -------------------------------------------*/
/*----------------------------------------------------------------------------*/
typedef struct PicamEMCalibrationDate
{
    piint year;
    piint month;
    piint day;
} PicamEMCalibrationDate;
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_GetCalibrationDate(
    PicamHandle             calibration,
    PicamEMCalibrationDate* value );
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_ReadSensorTemperatureReading(
    PicamHandle calibration,
    piflt*      value );
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_ReadSensorTemperatureStatus(
    PicamHandle                   calibration,
    PicamSensorTemperatureStatus* value );
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_GetSensorTemperatureSetPoint(
    PicamHandle calibration,
    piflt*      value );
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_SetSensorTemperatureSetPoint(
    PicamHandle calibration,
    piflt       value );
/*----------------------------------------------------------------------------*/
/* EM Calibration Parameter Constraints --------------------------------------*/
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_GetSensorTemperatureSetPointConstraint(
    PicamHandle                  calibration,
    const PicamRangeConstraint** constraint ); /* ALLOCATES */
/*----------------------------------------------------------------------------*/

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/* EM Calibration                                                             */
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
typedef pibln (PIL_CALL* PicamEMCalibrationCallback)(
    PicamHandle calibration,
    piflt       progress,
    void*       user_state );
/*----------------------------------------------------------------------------*/
PICAM_API PicamEMCalibration_Calibrate(
    PicamHandle                calibration,
    PicamEMCalibrationCallback calllback,
    void*                      user_state );
/*----------------------------------------------------------------------------*/

/******************************************************************************/
/* C++ Epilogue                                                               */
/******************************************************************************/
#if defined __cplusplus && !defined PICAM_EXPORTS
    }   /* end extern "C" */
#endif

#endif
