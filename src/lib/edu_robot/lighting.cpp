#include "edu_robot/lighting.hpp"

namespace eduart {
namespace robot {

Lighting::Lighting(const std::string& name, const Color default_color, const float default_brightness)
  : _name(name)
{
  (void)default_color;
  (void)default_brightness;
}

void Lighting::setColor(const Color color, const Mode mode)
{
  processSetColor(color, mode);
  _color = color;
}

void Lighting::setBrightness(const float brightness)
{
  processSetBrightness(brightness);
  _brightness = brightness;
}

} // end namespace eduart
} // end namespace robot
