#! /usr/bin/env python
# -*- coding: utf-8 -*-
"""
    用于测试 flb 的 INI 文件格式能否正确解析

    1 不能使用 内置的 ConfigParser 正确解析
    2 需要使用 pylex (lpeg 的解析算法)
"""
import ConfigParser
import sys


fname = sys.argv[1]


config = ConfigParser.ConfigParser()
config.read(fname)

print config.keys()

# end of file