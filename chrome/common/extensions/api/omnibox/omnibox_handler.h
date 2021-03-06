// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_OMNIBOX_OMNIBOX_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_OMNIBOX_OMNIBOX_HANDLER_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

class Extension;

struct OmniboxInfo : public Extension::ManifestData {
  // The Omnibox keyword for an extension.
  std::string keyword;

  // Returns the omnibox keyword for the extension.
  static const std::string& GetKeyword(const Extension* extension);
};

// Parses the "omnibox" manifest key.
class OmniboxHandler : public ManifestHandler {
 public:
  OmniboxHandler();
  ~OmniboxHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;

 private:
  const std::vector<std::string> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_OMNIBOX_OMNIBOX_HANDLER_H_
