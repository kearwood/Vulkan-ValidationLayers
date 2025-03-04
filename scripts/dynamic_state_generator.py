#!/usr/bin/python3 -i
#
# Copyright (c) 2023 The Khronos Group Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
from generator import *
from common_codegen import *

# This is a workaround to use a Python 2.7 and 3.x compatible syntax
from io import open

class DynamicStateOutputGeneratorOptions(GeneratorOptions):
    def __init__(self,
                 conventions = None,
                 filename = None,
                 directory = '.',
                 genpath = None,
                 apiname = 'vulkan',
                 profile = None,
                 versions = '.*',
                 emitversions = '.*',
                 defaultExtensions = 'vulkan',
                 addExtensions = None,
                 removeExtensions = None,
                 emitExtensions = None,
                 emitSpirv = None,
                 emitFormats = None,
                 sortProcedure = regSortFeatures,
                 genFuncPointers = True,
                 protectFile = True,
                 protectFeature = False,
                 apicall = 'VKAPI_ATTR ',
                 apientry = 'VKAPI_CALL ',
                 apientryp = 'VKAPI_PTR *',
                 indentFuncProto = True,
                 indentFuncPointer = False,
                 alignFuncParam = 48,
                 expandEnumerants = False):
        GeneratorOptions.__init__(self,
                conventions = conventions,
                filename = filename,
                directory = directory,
                genpath = genpath,
                apiname = apiname,
                profile = profile,
                versions = versions,
                emitversions = emitversions,
                defaultExtensions = defaultExtensions,
                addExtensions = addExtensions,
                removeExtensions = removeExtensions,
                emitExtensions = emitExtensions,
                emitSpirv = emitSpirv,
                emitFormats = emitFormats,
                sortProcedure = sortProcedure)
        self.genFuncPointers = genFuncPointers
        self.protectFile     = protectFile
        self.protectFeature  = protectFeature
        self.apicall         = apicall
        self.apientry        = apientry
        self.apientryp       = apientryp
        self.indentFuncProto = indentFuncProto
        self.indentFuncPointer = indentFuncPointer
        self.alignFuncParam  = alignFuncParam
        self.expandEnumerants = expandEnumerants
#
# DynamicStateOutputGenerator - Generate SPIR-V validation
# for SPIR-V extensions and capabilities
class DynamicStateOutputGenerator(OutputGenerator):
    def __init__(self,
                 errFile = sys.stderr,
                 warnFile = sys.stderr,
                 diagFile = sys.stdout):
        OutputGenerator.__init__(self, errFile, warnFile, diagFile)
        self.headerFile = False # Header file generation flag
        self.sourceFile = False # Source file generation flag

        self.dynamic_states = [] # VkDynamicState enum values

    #
    # Called at beginning of processing as file is opened
    def beginFile(self, genOpts):
        OutputGenerator.beginFile(self, genOpts)
        self.headerFile = (genOpts.filename == 'dynamic_state_helper.h')
        self.sourceFile = (genOpts.filename == 'dynamic_state_helper.cpp')

        # File Comment
        file_comment = '// *** THIS FILE IS GENERATED - DO NOT EDIT ***\n'
        file_comment += '// See dynamic_state_generator.py for modifications\n'
        write(file_comment, file=self.outFile)
        # Copyright Statement
        copyright = ''
        copyright += '\n'
        copyright += '/***************************************************************************\n'
        copyright += ' *\n'
        copyright += ' * Copyright (c) 2023 Valve Corporation\n'
        copyright += ' * Copyright (c) 2023 LunarG, Inc.\n'
        copyright += ' *\n'
        copyright += ' * Licensed under the Apache License, Version 2.0 (the "License");\n'
        copyright += ' * you may not use this file except in compliance with the License.\n'
        copyright += ' * You may obtain a copy of the License at\n'
        copyright += ' *\n'
        copyright += ' *     http://www.apache.org/licenses/LICENSE-2.0\n'
        copyright += ' *\n'
        copyright += ' * Unless required by applicable law or agreed to in writing, software\n'
        copyright += ' * distributed under the License is distributed on an "AS IS" BASIS,\n'
        copyright += ' * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n'
        copyright += ' * See the License for the specific language governing permissions and\n'
        copyright += ' * limitations under the License.\n'
        copyright += ' ****************************************************************************/\n'
        write(copyright, file=self.outFile)
        if self.sourceFile:
            write('#include "core_checks/core_validation.h"', file=self.outFile)
        elif self.headerFile:
            write('#pragma once', file=self.outFile)
            write('#include <bitset>', file=self.outFile)

    #
    # Write generated file content to output file
    def endFile(self):
        if self.headerFile:
            write(self.dynamicTypeEnum(), file=self.outFile)
        elif self.sourceFile:
            write(self.dynamicFunction(), file=self.outFile)
        # Finish processing in superclass
        OutputGenerator.endFile(self)

    #
    # List the enum for the commands
    def genGroup(self, groupinfo, name, alias):
        if (name == 'VkDynamicState'):
            for elem in groupinfo.elem.findall('enum'):
                if elem.get('alias') is None:
                    self.dynamic_states.append(elem.get('name'))

    #
    # List the enum for the dynamic command buffer status flags
    def dynamicTypeEnum(self):
        output = '''
// Reorders VkDynamicState so it can be a bitset
typedef enum CBDynamicState {\n'''
        counter = 1
        for name in self.dynamic_states:
            state_name = name[11:] # VK_DYNAMIC_STATE_LINE_WIDTH -> STATE_LINE_WIDTH
            output += '    CB_DYNAMIC_{} = {},\n'.format(state_name, str(counter))
            counter += 1

        output += '    CB_DYNAMIC_STATE_STATUS_NUM = ' + str(counter)
        output += '''
} CBDynamicState;

using CBDynamicFlags = std::bitset<CB_DYNAMIC_STATE_STATUS_NUM>;
CBDynamicState ConvertToCBDynamicState(VkDynamicState dynamic_state);
const char* DynamicStateToString(CBDynamicState dynamic_state);
std::string DynamicStatesToString(CBDynamicFlags const &dynamic_states);
'''
        return output

    #
    # List the enum for the dynamic command buffer status flags
    def dynamicFunction(self):
        output = '''
static VkDynamicState ConvertToDynamicState(CBDynamicState dynamic_state) {
    switch (dynamic_state) {\n'''
        for name in self.dynamic_states:
            state_name = name[11:] # VK_DYNAMIC_STATE_LINE_WIDTH -> STATE_LINE_WIDTH
            output += '        case CB_DYNAMIC_{}:\n'.format(state_name)
            output += '            return {};\n'.format(name)
        output += '''        default:
            return VK_DYNAMIC_STATE_MAX_ENUM;
    }
}
'''
        output += '''
CBDynamicState ConvertToCBDynamicState(VkDynamicState dynamic_state) {
    switch (dynamic_state) {\n'''
        for name in self.dynamic_states:
            state_name = name[11:] # VK_DYNAMIC_STATE_LINE_WIDTH -> STATE_LINE_WIDTH
            output += '        case {}:\n'.format(name)
            output += '            return CB_DYNAMIC_{};\n'.format(state_name)
        output += '''        default:
            return CB_DYNAMIC_STATE_STATUS_NUM;
    }
}
'''

        output += '''
const char* DynamicStateToString(CBDynamicState dynamic_state) {
    return string_VkDynamicState(ConvertToDynamicState(dynamic_state));
}

std::string DynamicStatesToString(CBDynamicFlags const &dynamic_states) {
    std::string ret;
    // enum is not zero based
    for (int index = 1; index < CB_DYNAMIC_STATE_STATUS_NUM; ++index) {
        CBDynamicState status = static_cast<CBDynamicState>(index);
        if (dynamic_states[status]) {
            if (!ret.empty()) ret.append("|");
            ret.append(string_VkDynamicState(ConvertToDynamicState(status)));
        }
    }
    if (ret.empty()) ret.append(string_VkDynamicState(ConvertToDynamicState(CB_DYNAMIC_STATE_STATUS_NUM)));
    return ret;
}
'''
        return output
