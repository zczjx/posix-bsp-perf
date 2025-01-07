#ifndef IMUXER_HPP
#define IMUXER_HPP

class IMuxer {
public:
    virtual ~IMuxer() = default;

    // Pure virtual function to be implemented by derived classes
    virtual void mux() = 0;
};

// Factory function to create instances of IMuxer
IMuxer* createMuxer();

#endif // IMUXER_HPP