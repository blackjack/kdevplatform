/* This file is part of KDevelop
   Copyright 2012 Olivier de Gaalon <olivier.jg@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef DELAYEDOUTPUT_H
#define DELAYEDOUTPUT_H

#include <QStack>
#include <QPair>
#include "tests/kdevplatformtestsexport.h"

namespace KDevelop
{

///Used to invert and visually nest error output generated by nested/recursive functions
///Used in TestSuite.h, use only at your own, singleton educated risk
class DelayedOutputPrivate;
class KDEVPLATFORMTESTS_EXPORT DelayedOutput
{
public:
  class Delay
  {
  public:
    Delay(DelayedOutput* output);
    ~Delay();
  private:
    DelayedOutput *m_output;
  };
  ~DelayedOutput();
  static DelayedOutput& self();
  void push(const QString &output);
private:
  DelayedOutput();
  Q_DISABLE_COPY(DelayedOutput);
  const QScopedPointer<DelayedOutputPrivate> d;
};

}

#endif //DELAYEDOUTPUT_H