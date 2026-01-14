#include <fstream>
#include <iostream>
#include <map>
#include <memory>

#include "Poco/AutoPtr.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "data.h"
#include "standx_client.h"
#include "strategy.h"
#include "tracer.h"
#include "util.h"

Config kConfig;
using Poco::AutoPtr;
using Poco::NotFoundException;
using Poco::Util::AbstractConfiguration;
using Poco::Util::PropertyFileConfiguration;

void InitConfig() {
  try {
    Poco::File dir("log");
    if (!dir.exists()) {
      dir.createDirectories();
    }
    AutoPtr<PropertyFileConfiguration> config =
        new PropertyFileConfiguration("config.properties");
    kConfig.uid = config->getString("uid");
    kConfig.secretKey = config->getString("secretKey");
    kConfig.chain = config->getString("chain");
    kConfig.lever = config->getDouble("order.lever");
    kConfig.minAvailBal = config->getDouble("order.minAvailBal");
    kConfig.whiteList = config->getString("order.whiteList");

    kConfig.logName = config->getString("log.logName");
    kConfig.logSize = config->getString("log.logSize");
    kConfig.logLevel = config->getString("log.logLevel");

    kConfig.barkServer = config->getString("bark.server");
    kConfig.subBtcSize = config->getDouble("sub.btcSize");
    kConfig.subEthSize = config->getDouble("sub.ethSize");
    kConfig.subSolSize = config->getDouble("sub.solSize");
    kConfig.gridLong = config->getBool("grid.long");
    kConfig.gridShort = config->getBool("grid.short");

    logger::Tracer::Init("default", kConfig.logName, kConfig.logSize);
    logger::Tracer::Init("api", "log/api.log", kConfig.logSize);
    logger::Tracer::SetLevel(kConfig.logLevel);
  } catch (NotFoundException& e) {
    std::cerr << "Config not found: " << e.what() << std::endl;
    exit(-1);
  }
}

int main() {
  InitConfig();
  NOTICE("standx start");

  std::string chain = kConfig.chain;
  std::string private_key = kConfig.secretKey;

  auto client = std::make_shared<standx::StandXClient>(chain, private_key,
                                                       kConfig.whiteList);
  auto strategy = std::make_shared<Strategy>(client);
  strategy->start();

  while (1) {
    SLEEP_MS(1000);
  }

  return 0;
}
