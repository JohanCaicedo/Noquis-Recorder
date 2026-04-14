#pragma once

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Inicia el puente de audio (capturadora USB -> audífonos)
    bool start();

    // Detiene el audio
    void stop();

private:
    void* miniaudioDevice; 
    bool isRunning;
};
