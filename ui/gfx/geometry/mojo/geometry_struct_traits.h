// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_MOJO_GEOMETRY_STRUCT_TRAITS_H_
#define UI_GFX_GEOMETRY_MOJO_GEOMETRY_STRUCT_TRAITS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/mojo/geometry.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::Insets, gfx::Insets> {
  static int top(const gfx::Insets& p) { return p.top(); }
  static int left(const gfx::Insets& p) { return p.left(); }
  static int bottom(const gfx::Insets& p) { return p.bottom(); }
  static int right(const gfx::Insets& p) { return p.right(); }
  static bool Read(gfx::mojom::InsetsDataView data, gfx::Insets* out) {
    out->Set(data.top(), data.left(), data.bottom(), data.right());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::InsetsF, gfx::InsetsF> {
  static float top(const gfx::InsetsF& p) { return p.top(); }
  static float left(const gfx::InsetsF& p) { return p.left(); }
  static float bottom(const gfx::InsetsF& p) { return p.bottom(); }
  static float right(const gfx::InsetsF& p) { return p.right(); }
  static bool Read(gfx::mojom::InsetsFDataView data, gfx::InsetsF* out) {
    out->Set(data.top(), data.left(), data.bottom(), data.right());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::Point, gfx::Point> {
  static int x(const gfx::Point& p) { return p.x(); }
  static int y(const gfx::Point& p) { return p.y(); }
  static bool Read(gfx::mojom::PointDataView data, gfx::Point* out) {
    out->SetPoint(data.x(), data.y());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::PointF, gfx::PointF> {
  static float x(const gfx::PointF& p) { return p.x(); }
  static float y(const gfx::PointF& p) { return p.y(); }
  static bool Read(gfx::mojom::PointFDataView data, gfx::PointF* out) {
    out->SetPoint(data.x(), data.y());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::Rect, gfx::Rect> {
  static int x(const gfx::Rect& p) { return p.x(); }
  static int y(const gfx::Rect& p) { return p.y(); }
  static int width(const gfx::Rect& p) { return p.width(); }
  static int height(const gfx::Rect& p) { return p.height(); }
  static bool Read(gfx::mojom::RectDataView data, gfx::Rect* out) {
    if (data.width() < 0 || data.height() < 0)
      return false;

    out->SetRect(data.x(), data.y(), data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::RectF, gfx::RectF> {
  static float x(const gfx::RectF& p) { return p.x(); }
  static float y(const gfx::RectF& p) { return p.y(); }
  static float width(const gfx::RectF& p) { return p.width(); }
  static float height(const gfx::RectF& p) { return p.height(); }
  static bool Read(gfx::mojom::RectFDataView data, gfx::RectF* out) {
    if (data.width() < 0 || data.height() < 0)
      return false;

    out->SetRect(data.x(), data.y(), data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::Size, gfx::Size> {
  static int width(const gfx::Size& p) { return p.width(); }
  static int height(const gfx::Size& p) { return p.height(); }
  static bool Read(gfx::mojom::SizeDataView data, gfx::Size* out) {
    if (data.width() < 0 || data.height() < 0)
      return false;

    out->SetSize(data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::SizeF, gfx::SizeF> {
  static float width(const gfx::SizeF& p) { return p.width(); }
  static float height(const gfx::SizeF& p) { return p.height(); }
  static bool Read(gfx::mojom::SizeFDataView data, gfx::SizeF* out) {
    if (data.width() < 0 || data.height() < 0)
      return false;

    out->SetSize(data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::Vector2d, gfx::Vector2d> {
  static int x(const gfx::Vector2d& v) { return v.x(); }
  static int y(const gfx::Vector2d& v) { return v.y(); }
  static bool Read(gfx::mojom::Vector2dDataView data, gfx::Vector2d* out) {
    out->set_x(data.x());
    out->set_y(data.y());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::Vector2dF, gfx::Vector2dF> {
  static float x(const gfx::Vector2dF& v) { return v.x(); }
  static float y(const gfx::Vector2dF& v) { return v.y(); }
  static bool Read(gfx::mojom::Vector2dFDataView data, gfx::Vector2dF* out) {
    out->set_x(data.x());
    out->set_y(data.y());
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_GEOMETRY_MOJO_GEOMETRY_STRUCT_TRAITS_H_
