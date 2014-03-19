# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'libutp_target_type%': 'static_library',
  },
  'target_defaults': {
    'defines': [
      'POSIX',
    ],
    'include_dirs': [
    ],
    'direct_dependent_settings': {
      'include_dirs': [
      ],
    },
    'conditions': [
    ],
  },
  'targets': [
    {
      'target_name': 'libutp',
      'type': 'static_library',
      'sources': [
        #'utp.h',
        'utp_internal.h',
        'utp_internal.cpp',
        'utp_utils.h',
        'utp_utils.cpp',
        'utp_hash.h',
        'utp_hash.cpp',
        'utp_callbacks.h',
        'utp_callbacks.cpp',
        'utp_api.cpp',
        'utp_packedsockaddr.h',
        'utp_packedsockaddr.cpp',
      ],  # sources
      'conditions': [
      ],  # conditions
    },  # target libutp
  ],  # targets
}
