/*
 * Copyright 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SwitchInputMapper.h"

#include <list>
#include <variant>

#include <NotifyArgs.h>
#include <gtest/gtest.h>
#include <input/Input.h>
#include <linux/input-event-codes.h>

#include "InputMapperTest.h"
#include "TestConstants.h"
#include "TestEventMatchers.h"

namespace android {

using testing::AllOf;
using testing::VariantWith;

class SwitchInputMapperTest : public InputMapperUnitTest {
protected:
    void SetUp() override {
        InputMapperUnitTest::SetUp();
        mMapper = createInputMapper<SwitchInputMapper>(*mDeviceContext,
                                                       mFakePolicy->getReaderConfiguration());
    }
};

TEST_F(SwitchInputMapperTest, GetSources) {
    ASSERT_EQ(uint32_t(AINPUT_SOURCE_SWITCH), mMapper->getSources());
}

TEST_F(SwitchInputMapperTest, GetSwitchState) {
    setSwitchState(1, {SW_LID});
    ASSERT_EQ(1, mMapper->getSwitchState(AINPUT_SOURCE_ANY, SW_LID));

    setSwitchState(0, {SW_LID});
    ASSERT_EQ(0, mMapper->getSwitchState(AINPUT_SOURCE_ANY, SW_LID));
}

TEST_F(SwitchInputMapperTest, Process) {
    process(ARBITRARY_TIME, EV_SW, SW_LID, 1);
    process(ARBITRARY_TIME, EV_SW, SW_JACK_PHYSICAL_INSERT, 1);
    process(ARBITRARY_TIME, EV_SW, SW_HEADPHONE_INSERT, 0);
    mFakeListener.assertNoEvents();

    process(ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    mFakeListener.expectSwitchEvent(
            AllOf(WithEventTime(ARBITRARY_TIME),
                  WithSwitchValues((1U << SW_LID) | (1U << SW_JACK_PHYSICAL_INSERT)),
                  WithSwitchMask((1U << SW_LID) | (1U << SW_JACK_PHYSICAL_INSERT) |
                                 (1 << SW_HEADPHONE_INSERT)),
                  WithPolicyFlags(0U)));
}

TEST_F(SwitchInputMapperTest, ResetReportsSwitchesChangedWhileEventsWereDropped) {
    // The mapper was created (and first reset) with every switch off: nothing is reported.
    mFakeListener.assertNoEvents();

    // The reader drops a frame after SYN_DROPPED and then resets the device.
    setSwitchState(1, {SW_PEN_INSERTED});
    processArgs(mMapper->reset(ARBITRARY_TIME));
    mFakeListener.expectSwitchEvent(AllOf(WithEventTime(ARBITRARY_TIME),
                                          WithSwitchValues(1U << SW_PEN_INSERTED),
                                          WithSwitchMask(1U << SW_PEN_INSERTED)));

    // Another reset with no change reports nothing.
    processArgs(mMapper->reset(ARBITRARY_TIME));
    mFakeListener.assertNoEvents();
}

TEST_F(SwitchInputMapperTest, FirstResetOnlyTakesTheState) {
    setSwitchState(1, {SW_PEN_INSERTED});
    std::unique_ptr<SwitchInputMapper> mapper =
            createInputMapper<SwitchInputMapper>(*mDeviceContext,
                                                 mFakePolicy->getReaderConfiguration());

    // Created with the switch on: the next reset reports nothing.
    processArgs(mapper->reset(ARBITRARY_TIME));
    mFakeListener.assertNoEvents();
}

} // namespace android