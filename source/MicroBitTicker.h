#include "MicroBitConfig.h"
#include "MicroBitComponent.h"

class MicroBitTicker : public MicroBitComponent
{
    public:
    MicroBitTicker();
    virtual void systemTick();
    virtual ~MicroBitTicker();
};


