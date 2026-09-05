#pragma once

// Stands in for the app's generated JuceHeader in framework-free test tools,
// so app sources that only want the logging macros compile as-is
#define Z_ERR(x) do {} while ( 0 )
#define Z_LOG(x) do {} while ( 0 )
#define Z_INFO(x) do {} while ( 0 )
