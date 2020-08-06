/*
  ==============================================================================

    AsynchronousValueTreeSynchroniser.h
    Created: 4 Aug 2020 8:58:13am
    Author:  L.Goodacre
    
    This class can be used to make shadow of a ValueTree in a thread safe way.
    ValueTreeSynchroniser is used to pick up the changes made to a ValueTree,
    which are then placed into a lock free FiFo. You can then update the shadow
    ValueTree from another thread by calling updateShadowValueTree().
    
    It hasn't been rigorously tested and there may be mistakes. Please let me
    know if you spot any.

  ==============================================================================
*/

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include <array>


template <int CAPACITY>
class AsyncValueTreeSynchroniser : public ValueTreeSynchroniser
{
public:
    AsyncValueTreeSynchroniser(const ValueTree& vt) :
        ValueTreeSynchroniser(vt), fifo(CAPACITY)
    {}

    inline bool updateShadowValueTree()
    {
        if (hasOverrun.get())
        {
            DBG("resetting ValueTree because of buffer overflow.");

            fifo.reset();
            hasOverrun.set(false);
            sendFullSyncCallback();
        }

        bool hasUpdatedSomething = fifo.getNumReady();

        const auto updateShadowVT = [this](const MemoryBlock& block)
        { applyChange(shadowValueTree, block.getData(), block.getSize(), nullptr); };

        while (fifo.getNumReady() > 0)
        {
            int start1, size1, start2, size2;
            fifo.prepareToRead(1, start1, size1, start2, size2);

            jassert(size1 + size2 <= 1);

            if (size1 > 0)
                updateShadowVT(arrayOfMemoryBlocks[start1]);

            else if (size2 > 0)
                updateShadowVT(arrayOfMemoryBlocks[start2]);

            fifo.finishedRead(size1 + size2);
        }
        return hasUpdatedSomething;
    }

    inline ValueTree getShaddowValueTree() const
    { return shadowValueTree; }

private:

    inline void stateChanged(const void* encodedChange, std::size_t sizeOfData) override
    {
        if (fifo.getFreeSpace() < 1)
        {
            hasOverrun.set(true);
            return;
        }
        else
        {
            int start1, size1, start2, size2;
            fifo.prepareToWrite(1, start1, size1, start2, size2);

            jassert(size1 + size2 <= 1);

            if (size1 > 0)
                arrayOfMemoryBlocks[start1] = MemoryBlock(encodedChange, sizeOfData);

            else if (size2 > 0)
                arrayOfMemoryBlocks[start2] = MemoryBlock(encodedChange, sizeOfData);

            fifo.finishedWrite(size1 + size2);
        }
    }
    
    ValueTree shadowValueTree;
    std::array<MemoryBlock, CAPACITY> arrayOfMemoryBlocks;
    AbstractFifo fifo;
    Atomic<bool> hasOverrun = false;
};
