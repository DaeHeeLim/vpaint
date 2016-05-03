// Copyright (C) 2012-2016 The VPaint Developers.
// See the COPYRIGHT file at the top-level directory of this distribution
// and at https://github.com/dalboris/vpaint/blob/master/COPYRIGHT
//
// This file is part of VPaint, a vector graphics editor. It is subject to the
// license terms and conditions in the LICENSE.MIT file found in the top-level
// directory of this distribution and at http://opensource.org/licenses/MIT

#include "View2D.h"

#include "Scene/SceneRenderer.h"
#include "Views/TestAction.h"
#include "Views/View2DRenderer.h"
#include "Tools/Sketch/SketchAction.h"

View2D::View2D(Scene * scene,
               SceneRendererSharedResources * sceneRendererSharedResources,
               QWidget * parent):
    View(scene, parent),
    sceneRendererSharedResources_(sceneRendererSharedResources)
{
    createRenderers_();
    addActions_();
}

View2DMouseEvent * View2D::makeMouseEvent()
{
    return new View2DMouseEvent(this);
}

void View2D::createRenderers_()
{
    sceneRenderer_ = new SceneRenderer(sceneRendererSharedResources_, this);
    view2DRenderer_ = new View2DRenderer(sceneRenderer_, this);
    setRenderer(view2DRenderer_);

}

void View2D::addActions_()
{
    addMouseAction(new TestAction(scene()));
    addMouseAction(new SketchAction(scene()));
}