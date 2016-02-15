#include <map>
#include <string>

namespace oclacc {

enum Vendor {
  ALTERA,
  XILINX
};

struct FPGA {
  Vendor V;
  size_t LogicElements;
  size_t OnChipRam;
  size_t DSPBlocks;

  FPGA(Vendor V, size_t LogicElements, size_t OnChipRam, size_t DSPBlocks) 
    : V(V), LogicElements(LogicElements), OnChipRam(OnChipRam), DSPBlocks(DSPBlocks) 
  { 
  }
};

struct DDR {
  size_t Size;
  size_t Freq;

  DDR(size_t Size, size_t Freq) : Size(Size), Freq(Freq) {}
};

struct TargetDevice {
  std::vector<FPGA> Devices;
  std::vector<DDR> RamBanks;

  TargetDevice() { }

  TargetDevice(FPGA &Device, DDR &RamBank)
    : Devices(), RamBanks() {
      Devices.push_back(Device);
      RamBanks.push_back(RamBank);
  }

  TargetDevice(FPGA &Device, std::vector<DDR> &RamBanks)
    : Devices(), RamBanks(RamBanks) {

    Devices.push_back(Device);
  }

  TargetDevice(std::vector<FPGA> &Devices, DDR &RamBank)
    : Devices(Devices), RamBanks() {

    RamBanks.push_back(RamBank);
  }

  TargetDevice(std::vector<FPGA> &Devices, std::vector<DDR> &RamBanks)
    : Devices(Devices), RamBanks(RamBanks) { }
};

static FPGA ALTERA_5SGSD5( ALTERA, 457000, 5155840, 1590 );
static DDR RAM_400MHz_4G(4*1024*1024, 400);
static TargetDevice BITTWARE_S5PHQ_D5( ALTERA_5SGSD5, RAM_400MHz_4G );

#if 0
/// \brief Manage all supported devices
///
class TargetDeviceRegistry {
  private:
    std::map<std::string, std::unique_ptr< TargetDevice > > Devices;

  public:
    TargetDeviceRegistry()  {}
};
#endif



}//ns oclacc
