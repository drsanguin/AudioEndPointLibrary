#include "DefSoundDeviceState.h"
#include "AudioEndPointLibraryImpl.h"


// This class is exported from the AudioEndPointLibrary.dll
namespace AudioEndPoint {

    extern "C" {
        AUDIOENDPOINTLIBRARY_API typedef struct {
            wchar_t* id;
            wchar_t* name;
            int is_default;
        } AudioEndpoint;

        AUDIOENDPOINTLIBRARY_API int get_all_audio_endpoints_count();
        AUDIOENDPOINTLIBRARY_API int get_all_audio_endpoints(AudioEndpoint* out_audio_endpoints, int audio_endpoints_count);
        AUDIOENDPOINTLIBRARY_API int set_default_audio_endpoint(wchar_t* id);
    }

    class AUDIOENDPOINTLIBRARY_API CAudioEndPointLibrary {
    public:
        ~CAudioEndPointLibrary();
        HRESULT OnDeviceStateChanged(LPCWSTR pwstr_device_id, DWORD dw_new_state) const;
        HRESULT OnDeviceAdded(LPCWSTR pwstr_device_id) const;
        HRESULT OnDeviceRemoved(LPCWSTR pwstr_device_id) const;
        HRESULT OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstr_default_device_id) const;
        static CAudioEndPointLibrary& GetInstance();
        AudioDeviceList GetPlaybackDevices(DefSound::EDeviceState state) const;
        AudioDeviceList GetRecordingDevices(DefSound::EDeviceState state) const;
        AudioDevicePtr GetAudioDevice(LPCWSTR id) const;

        AudioEndPointLibrarySignals* m_signals1() const
        {
            return m_signals;
        }

        __declspec(property(get = m_signals1)) AudioEndPointLibrarySignals* Signals;

    private:
        CAudioEndPointLibrary(void);
        CAudioEndPointLibrary(CAudioEndPointLibrary const&) = delete;
        void operator=(CAudioEndPointLibrary const&) = delete;

        struct AudioEndPointLibraryImpl;

        AudioEndPointLibraryImpl* m_container;
        AudioEndPointLibrarySignals* m_signals;
    };


}
