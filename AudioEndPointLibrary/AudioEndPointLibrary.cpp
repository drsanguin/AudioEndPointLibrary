// AudioEndPointLibrary.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AudioEndPointLibrary.h"
#include "MMNotificationClient.h"
#include <DefSoundEndpointColl.h>
#include <algorithm>
#include <mutex>

namespace AudioEndPoint {

    AUDIOENDPOINTLIBRARY_API int get_all_audio_endpoints_count()
    {
        try
        {
            auto endpoint_collection = DefSound::CEndpointCollection(DefSound::EDeviceState::Active);
            auto& endpoints = endpoint_collection.Get();

            return static_cast<int>(endpoints.size());
        }
        catch (...)
        {
            return -1;
        }
    }

    AUDIOENDPOINTLIBRARY_API int get_all_audio_endpoints(AudioEndpoint* out_audio_endpoints, int audio_endpoints_count)
    {
        try
        {
            auto endpoint_collection = DefSound::CEndpointCollection(DefSound::EDeviceState::Active);
            auto& endpoints = endpoint_collection.Get();

            for (auto i = 0; i < audio_endpoints_count; i++)
            {
                auto& endpoint = endpoints[i];

                out_audio_endpoints[i].id = new wchar_t[wcslen(endpoint.m_DeviceId.data()) + 1];
                wcscpy_s(out_audio_endpoints[i].id, wcslen(endpoint.m_DeviceId.data()) + 1, endpoint.m_DeviceId.data());

                out_audio_endpoints[i].name = new wchar_t[wcslen(endpoint.m_FriendlyName.data()) + 1];
                wcscpy_s(out_audio_endpoints[i].name, wcslen(endpoint.m_FriendlyName.data()) + 1, endpoint.m_FriendlyName.data());

                out_audio_endpoints[i].is_default = endpoint.m_IsDefault[ERole::eConsole] || endpoint.m_IsDefault[ERole::eMultimedia] ? 1 : 0;
            }

            return 0;
        }
        catch (...)
        {
            return -1;
        }
    }

    AUDIOENDPOINTLIBRARY_API int set_default_audio_endpoint(wchar_t* id) {
        try
        {
            auto endpoint_collection = DefSound::CEndpointCollection(DefSound::EDeviceState::Active);
            auto& endpoints = endpoint_collection.Get();

            for each(auto endpoint in endpoints)
            {
                if (wcscmp(endpoint.m_DeviceId.data(), id) != 0)
                {
                    continue;
                }

                endpoint_collection.SetDefaultEndpoint(endpoint, ERole::eConsole);
                endpoint_collection.SetDefaultEndpoint(endpoint, ERole::eMultimedia);
            }

            return 0;
        }
        catch (...)
        {
            return -1;
        }
    }

    struct CAudioEndPointLibrary::AudioEndPointLibraryImpl
    {
        IMMDeviceEnumeratorPtr m_DeviceEnumerator;
        IMMNotificationClientPtr m_notif_client;

        HRESULT RegisterNotificationClient();
        HRESULT UnRegisterNotificationClient() const;
        ~AudioEndPointLibraryImpl();
    };

    HRESULT CAudioEndPointLibrary::AudioEndPointLibraryImpl::RegisterNotificationClient()
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (!m_DeviceEnumerator)
        {
            hr = m_DeviceEnumerator.CreateInstance(__uuidof(MMDeviceEnumerator));
            ReturnIfFailed(hr);
        }

        if (!m_notif_client)
        {
            m_notif_client = new CMMNotificationClient;
        }

        hr = m_DeviceEnumerator->RegisterEndpointNotificationCallback(m_notif_client);
        return hr;
        
    }

    HRESULT CAudioEndPointLibrary::AudioEndPointLibraryImpl::UnRegisterNotificationClient() const
    {
        if (m_notif_client)
        {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            try
            {
                hr = m_DeviceEnumerator->UnregisterEndpointNotificationCallback(m_notif_client);
            }
            catch (...)
            {
            }
            return hr;
        }
        return S_FALSE;
    }

    CAudioEndPointLibrary::AudioEndPointLibraryImpl::~AudioEndPointLibraryImpl()
    {
        UnRegisterNotificationClient();
        m_DeviceEnumerator->Release();
        m_notif_client->Release();
    }

    // This is the constructor of a class that has been exported.
    // see AudioEndPointLibrary.h for the class definition
    CAudioEndPointLibrary::CAudioEndPointLibrary() : m_container(new AudioEndPointLibraryImpl()), 
        m_signals(new AudioEndPointLibrarySignals())
    {
        m_container->RegisterNotificationClient();
    }


    CAudioEndPointLibrary::~CAudioEndPointLibrary()
    {
        delete m_signals;
        delete m_container;
    }

    HRESULT CAudioEndPointLibrary::OnDeviceStateChanged(LPCWSTR pwstr_device_id, DWORD dw_new_state) const
    {
        auto device = this->GetAudioDevice(pwstr_device_id);
        auto e_device_state = static_cast<DefSound::EDeviceState>(dw_new_state);
        if (device != nullptr) {
            Signals->DeviceStateChanged.Notify(device, e_device_state);
        }
        return S_OK;
    }

    HRESULT CAudioEndPointLibrary::OnDeviceRemoved(LPCWSTR pwstr_device_id) const
    {
        DefSound::CEndpoint endpoint;
        std::unique_ptr < LPCWSTR, decltype(&::CoTaskMemFree) > DeviceIdHolder(&pwstr_device_id, &::CoTaskMemFree);
        endpoint.m_DeviceId = pwstr_device_id;
        Signals->DeviceRemoved.Notify(std::make_shared<AudioDevice>(endpoint, Playback));
        Signals->DeviceRemoved.Notify(std::make_shared<AudioDevice>(endpoint, Recording));

        return S_OK;
    }

    HRESULT CAudioEndPointLibrary::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstr_default_device_id) const
    {
        auto device = this->GetAudioDevice(pwstr_default_device_id);
        if (device != nullptr) {
            Signals->DeviceDefaultChanged.Notify(device, role);
        }     
        
        return S_OK;
    }

    HRESULT CAudioEndPointLibrary::OnDeviceAdded(LPCWSTR pwstr_device_id) const
    {
        auto device = this->GetAudioDevice(pwstr_device_id);
        if (device != nullptr) {
            Signals->DeviceAdded.Notify(device);
        }

        return S_OK;
    }

    CAudioEndPointLibrary& CAudioEndPointLibrary::GetInstance()
    {
        static CAudioEndPointLibrary instance;
        return instance;
    }

    AudioDeviceList CAudioEndPointLibrary::GetPlaybackDevices(DefSound::EDeviceState state) const
    {
        AudioDeviceList playbackList;
        auto collectionPlayback = DefSound::CEndpointCollection(state, ::eRender);
        for (auto &endpoint : collectionPlayback.Get())
        {
            playbackList.push_back(std::make_shared<AudioDevice>(endpoint, Playback));
        }
        return playbackList;     
    }

    AudioDeviceList CAudioEndPointLibrary::GetRecordingDevices(DefSound::EDeviceState state) const
    {
        AudioDeviceList recordList;
        auto collectionRecording = DefSound::CEndpointCollection(state, ::eCapture);
        for (auto &endpoint : collectionRecording.Get())
        {
            recordList.push_back(std::make_shared<AudioDevice>(endpoint, Recording));
        }
        return recordList;
    }

    AudioDevicePtr CAudioEndPointLibrary::GetAudioDevice(LPCWSTR pwstr_device_id) const
    {
        if (pwstr_device_id == nullptr) {
            return nullptr;
        }

        auto m_playback = this->GetPlaybackDevices(DefSound::EDeviceState::All);      
        auto audio_device = std::find_if(m_playback.begin(), m_playback.end(), [pwstr_device_id](AudioDevicePtr device) {
            return wcscmp(device->ID, pwstr_device_id) == 0;
        });

        if (audio_device != m_playback.end())
        {
            return *audio_device;
        }

        auto m_recording = this->GetRecordingDevices(DefSound::EDeviceState::All);
        audio_device = std::find_if(m_recording.begin(), m_recording.end(), [pwstr_device_id](AudioDevicePtr device) {
            return wcscmp(device->ID, pwstr_device_id) == 0;
        });

        if (audio_device != m_recording.end())
        {
            return *audio_device;
        }

        return nullptr;

    }
}
