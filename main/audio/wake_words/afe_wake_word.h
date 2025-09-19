#ifndef AFE_WAKE_WORD_H
#define AFE_WAKE_WORD_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_afe_sr_models.h>
#include <esp_nsn_models.h>
#include <model_path.h>

#include <deque>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "audio_codec.h"
#include "wake_word.h"

class AfeWakeWord : public WakeWord {
public:
    AfeWakeWord();
    ~AfeWakeWord();

    bool Initialize(AudioCodec* codec, srmodel_list_t* models_list);
    void Feed(const std::vector<int16_t>& data);
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback);
    void Start();
    void Stop();
    size_t GetFeedSize();
    void EncodeWakeWordData();
    bool GetWakeWordOpus(std::vector<uint8_t>& opus);
    const std::string& GetLastDetectedWakeWord() const { return last_detected_wake_word_; }

private:
    srmodel_list_t *models_ = nullptr;
    esp_afe_sr_iface_t* afe_iface_ = nullptr;
    esp_afe_sr_data_t* afe_data_ = nullptr;
    char* wakenet_model_ = NULL;
    std::vector<std::string> wake_words_;
    EventGroupHandle_t event_group_;
    std::function<void(const std::string& wake_word)> wake_word_detected_callback_;
    AudioCodec* codec_ = nullptr;
    std::string last_detected_wake_word_;

    TaskHandle_t wake_word_encode_task_ = nullptr;
    StaticTask_t* wake_word_encode_task_buffer_ = nullptr;
    StackType_t* wake_word_encode_task_stack_ = nullptr;
    // Ring buffer for PCM frames (hot path: zero-allocation)
    int16_t* pcm_ring_buffer_ = nullptr;         // contiguous buffer: pcm_frames_capacity_ * pcm_frame_samples_
    int pcm_frame_samples_ = 0;                  // samples per frame from AFE fetch chunk size
    int pcm_frames_capacity_ = 0;                // number of frames can be stored (e.g., ~1s)
    int pcm_frames_count_ = 0;                   // current frames stored
    int pcm_write_index_ = 0;                    // next write slot index
    std::mutex pcm_ring_mutex_;
    std::deque<std::vector<uint8_t>> wake_word_opus_;
    std::mutex wake_word_mutex_;
    std::condition_variable wake_word_cv_;

    void StoreWakeWordData(const int16_t* data, size_t size);
    void AudioDetectionTask();
};

#endif
