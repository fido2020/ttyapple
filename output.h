#pragma once

struct Frame {
    // Actual pixel data of the frame
    uint8_t* data;
    // Timestamp of the frame in microseconds
    long usTimestamp;
};

class Output {
public:
    virtual void send_frame(Frame* frame) = 0;
    virtual Frame* acquire_frame() = 0;
};

class TTYOutput : public Output {
public:
    void send_frame(Frame* frame) override;
    Frame* acquire_frame() override;
};

class COutput : public Output {
public:
    void send_frame(Frame* frame) override;
    Frame* acquire_frame() override;
};

class UEFIOutput : public Output {
public:
    void send_frame(Frame* frame) override;
    Frame* acquire_frame() override;
};
