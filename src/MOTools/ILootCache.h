#ifndef MOTOOLS_ILOOTCACHE_H
#define MOTOOLS_ILOOTCACHE_H

#include "Loot.h"

#include <QString>

namespace MOTools
{

class ILootCache
{
public:



  virtual void clearAdditionalInformation() = 0;






  virtual void addLootReport(const QString& name, Loot::Plugin plugin) = 0;
};

}  // namespace MOTools

#endif  // MOTOOLS_ILOOTCACHE_H
