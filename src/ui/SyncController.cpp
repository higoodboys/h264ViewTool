#include "ui/SyncController.h"

namespace h264tool {

SyncController::SyncController(QObject* parent) : QObject(parent) {}

void SyncController::setFrameCount(int count) {
    if (count < 0) count = 0;
    if (frameCount_ == count) return;
    frameCount_ = count;
    emit frameCountChanged(frameCount_);
    if (current_ >= frameCount_)
        setCurrentFrame(frameCount_ > 0 ? frameCount_ - 1 : 0);
}

void SyncController::setCurrentFrame(int index) {
    if (frameCount_ <= 0) {
        if (current_ != 0) {
            current_ = 0;
            emit currentFrameChanged(0);
        }
        return;
    }
    if (index < 0) index = 0;
    if (index >= frameCount_) index = frameCount_ - 1;
    if (current_ == index) return;
    current_ = index;
    emit currentFrameChanged(current_);
}

void SyncController::nextFrame() { setCurrentFrame(current_ + 1); }
void SyncController::prevFrame() { setCurrentFrame(current_ - 1); }
void SyncController::firstFrame() { setCurrentFrame(0); }
void SyncController::lastFrame() { setCurrentFrame(frameCount_ - 1); }

void SyncController::setPlaying(bool on) {
    if (playing_ == on) return;
    playing_ = on;
    emit playingChanged(playing_);
}

void SyncController::togglePlay() { setPlaying(!playing_); }

} // namespace h264tool
