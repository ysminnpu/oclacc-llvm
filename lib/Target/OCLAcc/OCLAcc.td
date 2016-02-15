include "llvm/Target/Target.td"

//Debugging Features
//name attribute, value, description, implied Features

def FeatureF1 : SubtargetFeature <"f1", 
  "HasF1", "true", "Feature 1 Explanation", []>;

def FeatureF2 : SubtargetFeature <"f2", 
  "HasF2", "true", "Feature 2 Explanation", []>;

def FeatureF3 : SubtargetFeature <"f3", 
  "HasF3", "true", "Feature 3 Explanation", []>;

def FeatureF4 : SubtargetFeature <"f4", 
  "HasF4", "true", "Feature 4 Explanation", []>;

def FeatureF5 : SubtargetFeature <"f5", 
  "HasF5", "true", "Feature 5 Explanation", []>;

class FpgaBoard<string Name, list<SubtargetFeature> Features>
  : Processor<Name, NoItineraries, Features>;

def : FpgaBoard<"generic",[]>;
def : FpgaBoard<"s5phq_d5",[FeatureF1]>;
def : FpgaBoard<"max2_22336b",[FeatureF2]>;
def : FpgaBoard<"proc2",[FeatureF1, FeatureF2, FeatureF3]>;
def : FpgaBoard<"proc3",[FeatureF1, FeatureF2, FeatureF3, FeatureF4]>;
def : FpgaBoard<"proc4",[FeatureF1, FeatureF2, FeatureF3, FeatureF4, FeatureF5]>;

def OCLAcc : Target {
}
