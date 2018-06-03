/*
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

/**
 *  @file
 *  Picolimno MKR V1.0 project
 *  parameters.h
 *  Manage all local parameters (volatile for now).
 *
 *  @author Marc Sibert
 *  @version 1.0 14/04/2018
 *  @Copyright 2018 Marc Sibert
 */

#pragma once

class Parameters {

private:
  struct Data {
    float limit1;
    float hyst1;
    float limit2;
    float hyst2;
    byte startTime;
    byte stopTime;
  };
  
  static const Data hardCoded;
  
  Data live;

protected:

public:
  Parameters() :
    live(hardCoded) {
  }

/*
  template<typename T>
  const T& operator[](const String& aName) const {
    
  }
*/

  float& limit1() {
    return live.limit1;
  }

};

const Parameters::Data Parameters::hardCoded = {
  0.0,
  0.0,
  0.0,
  0.0,
  0,
  0
};

