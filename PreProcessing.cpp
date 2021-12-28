#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#define LOG_TAG "aec_PreProcessing"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <utils/Timers.h>
#include <hardware/audio_effect.h>
#include <audio_effects/effect_aec.h>

//#define BRT_AEC_PROCESSING
//#define BRT_AEC_PROCESSING_REVERSE
//#define DEFAULT_VCP_SAMPLE_RATE 16000
#define DEFAULT_VCP_SAMPLE_RATE 8000
#define F_LOG ALOGV("%s", __FUNCTION__)

//#define DEBUG_OUTPUT_FILE
extern "C"
{
    //#include "vcp_internal.h"
    //#include "wav_file.h"

#define CONFIG_TUNNING_ONLINE

    typedef struct tag_vcp_effct
    {
        uint32_t sample_rate;
        uint32_t channels;
        uint32_t format;
        uint32_t reverse_sample_rate;
        uint32_t reverse_channels;
        uint32_t reverse_format;
        audio_buffer_t reverse_buf;
        uint8_t is_bt_audio;
    } vcp_effect_cb_t;

    static vcp_effect_cb_t s_vcp_cb;
    //WAV_FILE_HANDLE s_input_fp = NULL;
    //WAV_FILE_HANDLE s_output_fp = NULL;
    //WAV_FILE_HANDLE s_output_reverse_fp =NULL;
    // Acoustic Echo Cancellation

    static FILE *fpProcess = nullptr;
    static FILE *fpProcessReverse = nullptr;

    static const effect_descriptor_t sAecDescriptor = {{0x7b491460, 0x8d4d, 0x11e0, 0xbd61, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}}, // type
                                                       {0xbb392ec0, 0x8d4d, 0x11e0, 0xa896, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}}, // uuid
                                                       EFFECT_CONTROL_API_VERSION,
                                                       (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND),
                                                       0, //FIXME indicate CPU load
                                                       0, //FIXME indicate memory usage
                                                       "Acoustic Echo Canceler",
                                                       "The BRT Inc."};

    int set_config(
        effect_config_t *config)
    {
        uint32_t sr;
        uint32_t inCnl = popcount(config->inputCfg.channels);
        uint32_t outCnl = popcount(config->outputCfg.channels);
        uint32_t format = popcount(config->inputCfg.format);

        ALOGV("txz_audio set_config:in sample_rate %d channel %08x(%d) format", config->inputCfg.samplingRate, config->inputCfg.channels, inCnl, format);

        if (config->inputCfg.samplingRate != config->outputCfg.samplingRate || config->inputCfg.format != config->outputCfg.format || config->inputCfg.format != AUDIO_FORMAT_PCM_16_BIT)
        {
            return -EINVAL;
        }

        s_vcp_cb.sample_rate = config->inputCfg.samplingRate;
        s_vcp_cb.channels = inCnl;
        s_vcp_cb.format = config->inputCfg.format;

        return 0;
    }

    int set_reverse_config(
        effect_config_t *config)
    {
        uint32_t sr;
        uint32_t inCnl = popcount(config->inputCfg.channels);
        uint32_t outCnl = popcount(config->outputCfg.channels);
        uint32_t format = popcount(config->inputCfg.format);

        ALOGV("txz_audio set_reverse_config:in sample_rate %d channel %08x(%d) format = (%d)%d", config->inputCfg.samplingRate, config->inputCfg.channels, inCnl, config->inputCfg.format, format);

        s_vcp_cb.reverse_sample_rate = config->inputCfg.samplingRate;
        s_vcp_cb.reverse_channels = inCnl;
        s_vcp_cb.reverse_format = config->inputCfg.format;
        return 0;
    }

    //------------------------------------------------------------------------------
    // Effect Control Interface Implementation
    //------------------------------------------------------------------------------

    static unsigned char g_raw[320 * 2 * 2 * 2 * 2];

    static unsigned char g_last_ref_raw[320 * 2 * 2 * 2 * 2];

    static unsigned char g_countervail_buffer[16 * 2]; // g_countervail_buffer[16 * 2 * 8 * 2]; //16k 2chanal 8ms 16bit

    int
    PreProcessingFx_Process(
        effect_handle_t self,
        audio_buffer_t *inBuffer,
        audio_buffer_t *outBuffer)
    {
        ALOGD("txz_audio pid = %d", gettid());

        // ALOGV("txz PreProcessingFx_ProcessReverse");
        int status = 0;

        if (self == NULL)
        {
            ALOGV("txz_audio PreProcessingFx_Process() ERROR effect == NULL");
            return -EINVAL;
        }

        if (inBuffer == NULL || inBuffer->raw == NULL || outBuffer == NULL || outBuffer->raw == NULL)
        {
            ALOGW("txz_audio PreProcessingFx_Process() ERROR bad pointer");
            return -EINVAL;
        }

        //	if (s_input_fp != NULL) {
        //		wav_file_write(s_input_fp, inBuffer->raw, inBuffer->frameCount * s_vcp_cb.channels * 2);
        //	}

        /** TODO
     if (s_vcp_cb.sample_rate != DEFAULT_VCP_SAMPLE_RATE) {
     aec_resampler_down_resample(in_buf, &out_buf);
     } else {                                                                                                                                           out_buf.frameCount = in_buf->frameCount;
     out_buf.raw = in_buf->raw;
     }
     */
#ifdef BRT_AEC_PROCESSING
        vcp_process(inBuffer, outBuffer);
#else
        outBuffer->frameCount = inBuffer->frameCount;
        if (s_vcp_cb.sample_rate == 32000)
        {
            // memset(g_last_ref_raw, 0, sizeof(g_last_ref_raw));

            // memcpy(g_last_ref_raw, g_countervail_buffer, sizeof(g_countervail_buffer));

            // memcpy(g_last_ref_raw + sizeof(g_countervail_buffer), g_raw, inBuffer->frameCount * s_vcp_cb.channels - sizeof(g_countervail_buffer));

            // memcpy(g_countervail_buffer, g_raw + inBuffer->frameCount * s_vcp_cb.channels - sizeof(g_countervail_buffer), sizeof(g_countervail_buffer));

            for (size_t i = 0; i < inBuffer->frameCount * s_vcp_cb.channels; i = i + 4)
            {
                memcpy(((unsigned char *)outBuffer->raw) + (i * 2), (unsigned char *)g_raw + i, 4);
                memcpy(((unsigned char *)outBuffer->raw) + (i * 2 + 4), ((unsigned char *)inBuffer->raw) + i * 2, 4);
            }

            // memcpy(g_last_ref_raw, g_raw, sizeof(g_raw));
        }
        else if (s_vcp_cb.sample_rate == 16000)
        {
            for (size_t i = 0; i < inBuffer->frameCount * s_vcp_cb.channels; i = i + 2)
            {
                memcpy(((unsigned char *)outBuffer->raw) + (i * 2), (unsigned char *)g_raw + i, 2);
                memcpy(((unsigned char *)outBuffer->raw) + (i * 2 + 2), ((unsigned char *)inBuffer->raw) + i * 2, 2);
            }
        }
        else
        {
            memcpy(outBuffer->raw, inBuffer->raw, outBuffer->frameCount * s_vcp_cb.channels * 2);
        }

        if (fpProcess == nullptr)
        {
            fpProcess = fopen("/sdcard/txzProcess.pcm", "wb+");
            if (fpProcess == nullptr)
            {
                ALOGE("txz_audio fopen[%s] error[%d]:%s", "/sdcard/txzProcess.pcm", errno, strerror(errno));
                return status;
            }
        }
        ALOGD("txz_audio PreProcessingFx_Process frameCount = %d channels = %d", outBuffer->frameCount, s_vcp_cb.channels);
        fwrite(inBuffer->raw, 1, inBuffer->frameCount * s_vcp_cb.channels * 2, fpProcess);
        fflush(fpProcess);

#endif
        return status;
    }

#define CASE_RET(c) \
    case c:         \
        return #c;

    const char *get_command_str(
        int cmd)
    {
        switch (cmd)
        {
            CASE_RET(EFFECT_CMD_INIT);
            CASE_RET(EFFECT_CMD_SET_CONFIG);
            CASE_RET(EFFECT_CMD_RESET);
            CASE_RET(EFFECT_CMD_ENABLE);
            CASE_RET(EFFECT_CMD_DISABLE);
            CASE_RET(EFFECT_CMD_SET_PARAM);
            CASE_RET(EFFECT_CMD_SET_PARAM_DEFERRED);
            CASE_RET(EFFECT_CMD_SET_PARAM_COMMIT);
            CASE_RET(EFFECT_CMD_GET_PARAM);
            CASE_RET(EFFECT_CMD_SET_DEVICE);
            CASE_RET(EFFECT_CMD_SET_VOLUME);
            CASE_RET(EFFECT_CMD_SET_AUDIO_MODE);
            CASE_RET(EFFECT_CMD_SET_CONFIG_REVERSE);
            CASE_RET(EFFECT_CMD_SET_INPUT_DEVICE);
            CASE_RET(EFFECT_CMD_GET_CONFIG);
            CASE_RET(EFFECT_CMD_GET_CONFIG_REVERSE);
            CASE_RET(EFFECT_CMD_GET_FEATURE_SUPPORTED_CONFIGS);
            CASE_RET(EFFECT_CMD_GET_FEATURE_CONFIG);
            CASE_RET(EFFECT_CMD_SET_FEATURE_CONFIG);
            CASE_RET(EFFECT_CMD_SET_AUDIO_SOURCE);
            //CASE_RET(EFFECT_CMD_OFFLOAD);
        }

        return "unknown";
    }

    int PreProcessingFx_Command(
        effect_handle_t self,
        uint32_t cmdCode,
        uint32_t cmdSize,
        void *pCmdData,
        uint32_t *replySize,
        void *pReplyData)
    {
        int retsize;
        int status;

        if (self == NULL)
        {
            return -EINVAL;
        }

        if (cmdCode != EFFECT_CMD_SET_PARAM)
        {
            ALOGV("txz_audio PreProcessingFx_Command: command %s cmdSize %d", get_command_str(cmdCode), cmdSize);
        }

        switch (cmdCode)
        {
        case EFFECT_CMD_INIT:
            if (pReplyData == NULL || *replySize != sizeof(int))
            {
                return -EINVAL;
            }
            memset(&s_vcp_cb, 0, sizeof(s_vcp_cb));
            *(int *)pReplyData = 0;
            break;

        case EFFECT_CMD_SET_CONFIG:
        {
            effect_config_t *config = (effect_config_t *)pCmdData;
            if (pCmdData == NULL || cmdSize != sizeof(effect_config_t) || pReplyData == NULL || *replySize != sizeof(int))
            {
                ALOGV("txz_audio PreProcessingFx_Command cmdCode Case: "
                      "EFFECT_CMD_SET_CONFIG: ERROR");
                return -EINVAL;
            }

            *(int *)pReplyData = set_config(config);
        }
        break;

        case EFFECT_CMD_GET_CONFIG:
            break;

        case EFFECT_CMD_SET_CONFIG_REVERSE:
        {
            effect_config_t *config = (effect_config_t *)pCmdData;
            if (pCmdData == NULL || cmdSize != sizeof(effect_config_t) || pReplyData == NULL || *replySize != sizeof(int))
            {
                ALOGV("txz_audio PreProcessingFx_Command cmdCode Case: "
                      "EFFECT_CMD_SET_CONFIG: ERROR");
                return -EINVAL;
            }

            *(int *)pReplyData = set_reverse_config(config);
        }
        break;

        case EFFECT_CMD_GET_CONFIG_REVERSE:
            break;

        case EFFECT_CMD_RESET:
            break;

        case EFFECT_CMD_GET_PARAM:
        {
            if (pCmdData == NULL || cmdSize < (int)sizeof(effect_param_t) || pReplyData == NULL || *replySize < (int)sizeof(effect_param_t))
            {
                ALOGV("txz_audio PreProcessingFx_Command cmdCode Case: "
                      "EFFECT_CMD_GET_PARAM: ERROR");
                return -EINVAL;
            }
            effect_param_t *p = (effect_param_t *)pCmdData;
            memcpy(pReplyData, pCmdData, sizeof(effect_param_t) + p->psize);

            p = (effect_param_t *)pReplyData;
            int voffset = ((p->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);
            // TODO
        }
        break;

        case EFFECT_CMD_SET_PARAM:
        {
            if (pCmdData == NULL || cmdSize < (int)sizeof(effect_param_t) || pReplyData == NULL || *replySize != sizeof(int32_t))
            {
                ALOGV("txz_audio PreProcessingFx_Command cmdCode Case: "
                      "EFFECT_CMD_SET_PARAM: ERROR");
                return -EINVAL;
            }
            effect_param_t *p = (effect_param_t *)pCmdData;

            if (p->psize != sizeof(int32_t))
            {
                ALOGV("txz_audio PreProcessingFx_Command cmdCode Case: "
                      "EFFECT_CMD_SET_PARAM: ERROR, psize is not sizeof(int32_t)");
                return -EINVAL;
            }
        }
        break;

        case EFFECT_CMD_ENABLE:
            if (pReplyData == NULL || *replySize != sizeof(int))
            {
                ALOGV("txz_audio PreProcessingFx_Command cmdCode Case: EFFECT_CMD_ENABLE: ERROR");
                return -EINVAL;
            }

#ifdef DEBUG_OUTPUT_FILE
            s_input_fp = wav_file_open("/data/brlink/hal_input.wav", s_vcp_cb.sample_rate, s_vcp_cb.channels, 16);
            s_output_fp = wav_file_open("/data/brlink/hal_output.wav", s_vcp_cb.sample_rate, s_vcp_cb.channels, 16);
            s_output_reverse_fp = wav_file_open("/data/brlink/hal_output_reverse.wav", s_vcp_cb.sample_rate, s_vcp_cb.channels, 16);
#endif
            //			vcp_create(s_vcp_cb.sample_rate, s_vcp_cb.channels);
            s_vcp_cb.reverse_buf.raw = malloc(4096);
            *(int *)pReplyData = 0;
            break;

        case EFFECT_CMD_DISABLE:
            if (pReplyData == NULL || *replySize != sizeof(int))
            {
                ALOGV("txz_audio PreProcessingFx_Command cmdCode Case: EFFECT_CMD_DISABLE: ERROR");
                return -EINVAL;
            }

            //			if (s_input_fp != NULL) {
            //				wav_file_close(s_input_fp);
            //				s_input_fp = NULL;
            //			}
            //			if (s_output_fp != NULL) {
            //				wav_file_close(s_output_fp);
            //				s_output_fp = NULL;
            //			}
            //			if (s_output_reverse_fp != NULL) {
            //				wav_file_close(s_output_reverse_fp);
            //				s_output_reverse_fp = NULL;
            //			}
            //			vcp_destroy();
            if (s_vcp_cb.reverse_buf.raw)
            {
                free(s_vcp_cb.reverse_buf.raw);
            }
            *(int *)pReplyData = 0;
            break;

        case EFFECT_CMD_SET_DEVICE:
        case EFFECT_CMD_SET_INPUT_DEVICE:
            if (pCmdData == NULL || cmdSize != sizeof(uint32_t))
            {
                ALOGV("txz_audio PreProcessingFx_Command cmdCode Case: EFFECT_CMD_SET_DEVICE: ERROR");
                return -EINVAL;
            }
            // TODO
            ALOGV("txz_audio set_device(0x%08X)", *(uint32_t *)pCmdData);
            break;

        case EFFECT_CMD_SET_VOLUME:
        case EFFECT_CMD_SET_AUDIO_MODE:
            break;

        default:
            return -EINVAL;
        }
        return 0;
    }

    int PreProcessingFx_GetDescriptor(
        effect_handle_t self,
        effect_descriptor_t *pDescriptor)
    {
        if (self == NULL || pDescriptor == NULL)
        {
            return -EINVAL;
        }

        *pDescriptor = sAecDescriptor;

        return 0;
    }

    int PreProcessingFx_ProcessReverse(
        effect_handle_t self,
        audio_buffer_t *inBuffer,
        audio_buffer_t *outBuffer)
    {
        //    ALOGW("txz PreProcessingFx_ProcessReverse");
        int status = 0;
        ALOGV("txz_audio pid = %d ", gettid());

        if (self == NULL)
        {
            ALOGW("txz_audio PreProcessingFx_ProcessReverse() ERROR effect == NULL");
            return -EINVAL;
        }

        if (inBuffer == NULL || inBuffer->raw == NULL)
        {
            ALOGW("txz_audio PreProcessingFx_ProcessReverse() ERROR bad pointer");
            return -EINVAL;
        }

        //	if (s_output_fp != NULL) {
        //		wav_file_write(s_output_fp, inBuffer->raw, inBuffer->frameCount * s_vcp_cb.channels * 2);
        //	}

#ifdef BRT_AEC_PROCESSING

#ifdef BRT_AEC_PROCESSING_REVERSE
        if (outBuffer == NULL)
        {
            outBuffer = &s_vcp_cb.reverse_buf;
        }
        vcp_process_reverse(inBuffer, outBuffer);
        if (s_output_reverse_fp != NULL)
        {
            wav_file_write(s_output_reverse_fp, outBuffer->raw, outBuffer->frameCount * s_vcp_cb.channels * 2);
        }
        vcp_set_echo_reference(outBuffer);
#else
        vcp_set_echo_reference(inBuffer);
#endif

#endif

        if (fpProcessReverse == nullptr)
        {
            fpProcessReverse = fopen("/sdcard/txzProcessReverse.pcm", "wb+");
            if (fpProcessReverse == nullptr)
            {
                ALOGW("txz_audio fopen[%s] error[%d]:%s", "/sdcard/txzProcessReverse.pcm", errno, strerror(errno));
                return status;
            }
        }
        ALOGD("txz_audio PreProcessingFx_ProcessReverse g_countervail_buffer frameCount = %d channels = %d sample_rate = %d", inBuffer->frameCount, s_vcp_cb.channels, s_vcp_cb.sample_rate);
        if (s_vcp_cb.sample_rate == 32000)
        {
            ALOGD("g_countervail_buffer size new = %d", sizeof(g_countervail_buffer));
            memset(g_raw, 0, sizeof(g_raw));
            for (size_t i = 0; i < inBuffer->frameCount * s_vcp_cb.channels; i = i + 4)
            {
                memcpy(((unsigned char *)g_raw) + i, ((unsigned char *)inBuffer->raw) + i * 2, 4); //只取了双通道16k的数据---降采样
            }
            // memcpy(g_last_ref_raw, g_countervail_buffer, sizeof(g_countervail_buffer));
            // memcpy(g_last_ref_raw + sizeof(g_countervail_buffer), g_raw, inBuffer->frameCount * s_vcp_cb.channels - sizeof(g_countervail_buffer));
            // memcpy(g_countervail_buffer, g_raw + inBuffer->frameCount * s_vcp_cb.channels - sizeof(g_countervail_buffer), sizeof(g_countervail_buffer));

            // memcpy(g_last_ref_raw, g_raw, inBuffer->frameCount * s_vcp_cb.channels);
        }
        else if (s_vcp_cb.sample_rate == 16000)
        {
            memset(g_raw, 0, sizeof(g_raw));
            for (size_t i = 0; i < inBuffer->frameCount * s_vcp_cb.channels; i = i + 2)
            {
                memcpy(((unsigned char *)g_raw) + i, ((unsigned char *)inBuffer->raw) + i * 2, 2);
            }
        }

        else
        {
            // memcpy(outBuffer->raw, inBuffer->raw, inBuffer->frameCount * s_vcp_cb.channels * 2);
        }

        fwrite(inBuffer->raw, 1, inBuffer->frameCount * s_vcp_cb.channels * 2, fpProcessReverse);
        fflush(fpProcessReverse);

        return status;
    }

    const struct effect_interface_s sEffectInterfaceReverse = {PreProcessingFx_Process, PreProcessingFx_Command, PreProcessingFx_GetDescriptor,
                                                               PreProcessingFx_ProcessReverse};

    const struct effect_interface_s *itf = &sEffectInterfaceReverse;
    //------------------------------------------------------------------------------
    // Effect Library Interface Implementation
    //------------------------------------------------------------------------------

    // typedef struct effect_interface_s **effect_handle_t;
    int BRT_vcp_create(
        const effect_uuid_t *uuid,
        int32_t sessionId,
        int32_t ioId,
        effect_handle_t *pInterface)
    {
        ALOGV("txz_audio TXZ>>>>>>>>>>>>>>> uuid: %08x session %d IO: %d itfe:%p", uuid->timeLow, sessionId, ioId, pInterface);
        ALOGV("txz_audio EffectCreate: uuid: %08x session %d IO: %d itfe:%p", uuid->timeLow, sessionId, ioId, pInterface);

        int status = 0;
        const effect_descriptor_t *desc;
        uint32_t procId;

        if (pInterface != NULL)
            *pInterface = (effect_handle_t)&itf;

        return status;
    }

    int BRT_vcp_destroy(
        effect_handle_t interface)
    {
        int status = 0;
        ALOGV("txz_audio EffectRelease BRT_vcp_destroy %p", interface);
        fclose(fpProcessReverse);
        fpProcessReverse = nullptr;
        fclose(fpProcess);
        fpProcess = nullptr;
        // TODO
        return status;
    }

    int BRT_vcp_get_descriptor(
        const effect_uuid_t *uuid,
        effect_descriptor_t *pDescriptor)
    {

        if (pDescriptor == NULL || uuid == NULL)
        {
            return -EINVAL;
        }

        const effect_descriptor_t *desc = &sAecDescriptor;
        ALOGV("txz_audio vcp_get_descriptor() got fx %s", desc->name);
        *pDescriptor = *desc;
        return 0;
    }

    // This is the only symbol that needs to be exported
    __attribute__((visibility("default"))) audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {.tag = AUDIO_EFFECT_LIBRARY_TAG, .version = EFFECT_LIBRARY_API_VERSION, .name = "Audio Preprocessing Library", .implementor = "The BRT Inc.", .create_effect = BRT_vcp_create, .release_effect = BRT_vcp_destroy, .get_descriptor = BRT_vcp_get_descriptor};
};
// extern "C"
