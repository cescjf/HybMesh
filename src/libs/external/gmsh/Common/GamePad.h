// Gmsh - Copyright (C) 1997-2015 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to the public mailing list <gmsh@geuz.org>.
//
// Contributed by Gilles Marckmann <gilles.marckmann@ec-nantes.fr>

#ifndef _GAMEPAD_H_
#define _GAMEPAD_H_

#define GP_RANGE 1.0
#define GP_DEV 16
#define GP_BUTTONS 32
#define GP_AXES 6

#include "GmshConfig.h"

#if defined(WIN32)
#include <windows.h>
#include <mmsystem.h>
#elif defined(HAVE_LINUX_JOYSTICK)
#include <linux/joystick.h>
#include <fcntl.h>
#define GAMEPAD_DEV "/dev/input/js0"
#endif

class GamePad {
 public:
  bool active;
  bool toggle_status[GP_BUTTONS];
  bool event_read;
  double frequency;
  GamePad();
  ~GamePad();
  double axe[GP_AXES];
  bool button[GP_BUTTONS];
  bool toggle(const int _nbut);
  int read_event();
  void affiche();
  int button_map[10];
  int axe_map[8];
 private:
  char name[256];
#if defined(WIN32)
  int gamepad_fd;
  JOYCAPS caps;
  JOYINFOEX infoex;
  JOYINFO info;
  int axes;
  int buttons;
#elif defined(HAVE_LINUX_JOYSTICK)
  int gamepad_fd;
  js_event event;
  __u32 version;
  __u8 axes;
  __u8 buttons;
#else
  int axes;
  int buttons;
#endif
};

#endif
