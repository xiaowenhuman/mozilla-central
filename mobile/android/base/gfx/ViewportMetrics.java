/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.DisplayMetrics;
import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.RectUtils;
import org.json.JSONException;
import org.json.JSONObject;
import android.util.Log;

/**
 * ViewportMetrics manages state and contains some utility functions related to
 * the page viewport for the Gecko layer client to use.
 */
public class ViewportMetrics {
    private static final String LOGTAG = "GeckoViewportMetrics";

    private FloatSize mPageSize;
    private FloatSize mCssPageSize;
    private RectF mViewportRect;
    private float mZoomFactor;

    public ViewportMetrics() {
        DisplayMetrics metrics = new DisplayMetrics();
        GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);

        mPageSize = new FloatSize(metrics.widthPixels, metrics.heightPixels);
        mCssPageSize = new FloatSize(metrics.widthPixels, metrics.heightPixels);
        mViewportRect = new RectF(0, 0, metrics.widthPixels, metrics.heightPixels);
        mZoomFactor = 1.0f;
    }

    public ViewportMetrics(ViewportMetrics viewport) {
        mPageSize = new FloatSize(viewport.getPageSize());
        mCssPageSize = new FloatSize(viewport.getCssPageSize());
        mViewportRect = new RectF(viewport.getViewport());
        mZoomFactor = viewport.getZoomFactor();
    }

    public ViewportMetrics(ImmutableViewportMetrics viewport) {
        mPageSize = new FloatSize(viewport.pageSizeWidth, viewport.pageSizeHeight);
        mCssPageSize = new FloatSize(viewport.cssPageSizeWidth, viewport.cssPageSizeHeight);
        mViewportRect = new RectF(viewport.viewportRectLeft,
                                  viewport.viewportRectTop,
                                  viewport.viewportRectRight,
                                  viewport.viewportRectBottom);
        mZoomFactor = viewport.zoomFactor;
    }


    public ViewportMetrics(JSONObject json) throws JSONException {
        float x = (float)json.getDouble("x");
        float y = (float)json.getDouble("y");
        float width = (float)json.getDouble("width");
        float height = (float)json.getDouble("height");
        float pageWidth = (float)json.getDouble("pageWidth");
        float pageHeight = (float)json.getDouble("pageHeight");
        float cssPageWidth = (float)json.getDouble("cssPageWidth");
        float cssPageHeight = (float)json.getDouble("cssPageHeight");
        float zoom = (float)json.getDouble("zoom");

        mPageSize = new FloatSize(pageWidth, pageHeight);
        mCssPageSize = new FloatSize(cssPageWidth, cssPageHeight);
        mViewportRect = new RectF(x, y, x + width, y + height);
        mZoomFactor = zoom;
    }

    public PointF getOrigin() {
        return new PointF(mViewportRect.left, mViewportRect.top);
    }

    public FloatSize getSize() {
        return new FloatSize(mViewportRect.width(), mViewportRect.height());
    }

    public RectF getViewport() {
        return mViewportRect;
    }

    public RectF getCssViewport() {
        return RectUtils.scale(mViewportRect, 1/mZoomFactor);
    }

    /** Returns the viewport rectangle, clamped within the page-size. */
    public RectF getClampedViewport() {
        RectF clampedViewport = new RectF(mViewportRect);

        // While the viewport size ought to never exceed the page size, we
        // do the clamping in this order to make sure that the origin is
        // never negative.
        if (clampedViewport.right > mPageSize.width)
            clampedViewport.offset(mPageSize.width - clampedViewport.right, 0);
        if (clampedViewport.left < 0)
            clampedViewport.offset(-clampedViewport.left, 0);

        if (clampedViewport.bottom > mPageSize.height)
            clampedViewport.offset(0, mPageSize.height - clampedViewport.bottom);
        if (clampedViewport.top < 0)
            clampedViewport.offset(0, -clampedViewport.top);

        return clampedViewport;
    }

    public FloatSize getPageSize() {
        return mPageSize;
    }

    public FloatSize getCssPageSize() {
        return mCssPageSize;
    }


    public float getZoomFactor() {
        return mZoomFactor;
    }

    public void setPageSize(FloatSize pageSize, FloatSize cssPageSize) {
        mPageSize = pageSize;
        mCssPageSize = cssPageSize;
    }

    public void setViewport(RectF viewport) {
        mViewportRect = viewport;
    }

    public void setOrigin(PointF origin) {
        mViewportRect.set(origin.x, origin.y,
                          origin.x + mViewportRect.width(),
                          origin.y + mViewportRect.height());
    }

    public void setSize(FloatSize size) {
        mViewportRect.right = mViewportRect.left + size.width;
        mViewportRect.bottom = mViewportRect.top + size.height;
    }

    public void setZoomFactor(float zoomFactor) {
        mZoomFactor = zoomFactor;
    }

    /* This will set the zoom factor and re-scale page-size and viewport offset
     * accordingly. The given focus will remain at the same point on the screen
     * after scaling.
     */
    public void scaleTo(float newZoomFactor, PointF focus) {
        // mCssPageSize is invariant, since we're setting the scale factor
        // here. The page size is based on the CSS page size.
        mPageSize = mCssPageSize.scale(newZoomFactor);

        float scaleFactor = newZoomFactor / mZoomFactor;
        PointF origin = getOrigin();

        origin.offset(focus.x, focus.y);
        origin = PointUtils.scale(origin, scaleFactor);
        origin.offset(-focus.x, -focus.y);

        setOrigin(origin);

        mZoomFactor = newZoomFactor;
    }

    /*
     * Returns the viewport metrics that represent a linear transition between `from` and `to` at
     * time `t`, which is on the scale [0, 1). This function interpolates the viewport rect, the
     * page size, the offset, and the zoom factor.
     */
    public ViewportMetrics interpolate(ViewportMetrics to, float t) {
        ViewportMetrics result = new ViewportMetrics();
        result.mPageSize = mPageSize.interpolate(to.mPageSize, t);
        result.mCssPageSize = mCssPageSize.interpolate(to.mCssPageSize, t);
        result.mZoomFactor = FloatUtils.interpolate(mZoomFactor, to.mZoomFactor, t);
        result.mViewportRect = RectUtils.interpolate(mViewportRect, to.mViewportRect, t);
        return result;
    }

    public boolean fuzzyEquals(ViewportMetrics other) {
        return mPageSize.fuzzyEquals(other.mPageSize)
            && mCssPageSize.fuzzyEquals(other.mCssPageSize)
            && RectUtils.fuzzyEquals(mViewportRect, other.mViewportRect)
            && FloatUtils.fuzzyEquals(mZoomFactor, other.mZoomFactor);
    }

    public String toJSON() {
        // Round off height and width. Since the height and width are the size of the screen, it
        // makes no sense to send non-integer coordinates to Gecko.
        int height = Math.round(mViewportRect.height());
        int width = Math.round(mViewportRect.width());

        StringBuffer sb = new StringBuffer(256);
        sb.append("{ \"x\" : ").append(mViewportRect.left)
          .append(", \"y\" : ").append(mViewportRect.top)
          .append(", \"width\" : ").append(width)
          .append(", \"height\" : ").append(height)
          .append(", \"pageWidth\" : ").append(mPageSize.width)
          .append(", \"pageHeight\" : ").append(mPageSize.height)
          .append(", \"cssPageWidth\" : ").append(mCssPageSize.width)
          .append(", \"cssPageHeight\" : ").append(mCssPageSize.height)
          .append(", \"zoom\" : ").append(mZoomFactor)
          .append(" }");
        return sb.toString();
    }

    @Override
    public String toString() {
        StringBuffer buff = new StringBuffer(128);
        buff.append("v=").append(mViewportRect.toString())
            .append(" p=").append(mPageSize.toString())
            .append(" c=").append(mCssPageSize.toString())
            .append(" z=").append(mZoomFactor);
        return buff.toString();
    }
}

