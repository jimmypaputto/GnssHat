# HotStart Example

Performance comparison demonstration between cold start and hot start GPS acquisition.

## Overview

This example measures and compares the time required for GPS fix acquisition using two different startup methods:

- **Cold Start**: Clears all satellite data, requiring full satellite search and data download
- **Hot Start**: Preserves satellite data from previous session for faster acquisition

The example demonstrates the significant performance benefits of hot start when satellite data is available.

## Test Sequence

### 1. Initial Startup
- Configures GNSS HAT with optimized settings
- Validates successful initialization

### 2. Cold Start Test
- Performs `hardResetUbloxSom_ColdStart()`
- Clears all satellite data (almanac, ephemeris, position)
- Measures time until first valid fix
- Typically takes 25-45 seconds

### 3. Data Collection Phase
- Waits 40 seconds to collect fresh satellite data
- Allows receiver to download almanac and ephemeris
- Builds satellite constellation map

### 4. Hot Start Test  
- Performs `softResetUbloxSom_HotStart()`
- Preserves satellite data from previous session
- Measures time until fix reacquisition
- Typically takes 1-5 seconds

## Performance Analysis

### Typical Results

| Start Type | Expected Time | Description |
|------------|---------------|-------------|
| Cold Start | 25-45 seconds | Complete satellite search and data download |
| Hot Start | 1-2 seconds | Reuses existing satellite data |

### Performance Factors

**Cold Start Time Depends On:**
- Number of visible satellites
- Signal quality and atmospheric conditions
- Time since last satellite data download
- Receiver's almanac age

**Hot Start Time Depends On:**
- Data freshness (satellite data validity)
- Time since last valid fix
- Satellite constellation changes
- Signal availability

## Notes

- Test requires clear sky view for accurate results
- Performance varies with satellite constellation
- Hot start data expires after 4 hours without power
- Cold start is recommended after long power-off periods (>4 hours)
