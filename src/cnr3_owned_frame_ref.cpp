#include "cnr3_owned_frame_ref.h"

Cnr3OwnedFrameRef::~Cnr3OwnedFrameRef() noexcept {
    reset();
}

Cnr3OwnedFrameRef::Cnr3OwnedFrameRef(
    Cnr3OwnedFrameRef&& other
) noexcept
    : frame_(other.frame_),
    vsapi_(other.vsapi_) {
    other.frame_ = nullptr;
    other.vsapi_ = nullptr;
}

Cnr3OwnedFrameRef& Cnr3OwnedFrameRef::operator=(
    Cnr3OwnedFrameRef&& other
    ) noexcept {
    if (this != &other) {
        reset();

        frame_ = other.frame_;
        vsapi_ = other.vsapi_;

        other.frame_ = nullptr;
        other.vsapi_ = nullptr;
    }

    return *this;
}

const VSFrame* Cnr3OwnedFrameRef::get() const noexcept {
    return frame_;
}

bool Cnr3OwnedFrameRef::has_frame() const noexcept {
    return frame_ != nullptr;
}

Cnr3Status Cnr3OwnedFrameRef::reset_to_owned_frame(
    const VSFrame* frame,
    const VSAPI* vsapi
) noexcept {
    if (frame == nullptr) {
        reset();
        return Cnr3Status::ok;
    }

    if (vsapi == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    reset();

    frame_ = frame;
    vsapi_ = vsapi;

    return Cnr3Status::ok;
}

void Cnr3OwnedFrameRef::reset() noexcept {
    const VSFrame* frame_to_free = frame_;
    const VSAPI* vsapi_to_use = vsapi_;

    frame_ = nullptr;
    vsapi_ = nullptr;

    if (frame_to_free != nullptr && vsapi_to_use != nullptr) {
        vsapi_to_use->freeFrame(frame_to_free);
    }
}

const VSFrame* Cnr3OwnedFrameRef::transfer_to_caller() noexcept {
    const VSFrame* frame_to_transfer = frame_;

    frame_ = nullptr;
    vsapi_ = nullptr;

    return frame_to_transfer;
}
