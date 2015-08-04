// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.internal_api.pub.base;

import android.util.Log;

import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.protos.ipc.invalidation.Types;

import org.chromium.base.VisibleForTesting;

import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/**
 * The model types that are synced in Chrome for Android.
 */
public enum ModelType {
    /**
     * An autofill object.
     */
    AUTOFILL("AUTOFILL"),
    /**
     * An autofill profile object.
     */
    AUTOFILL_PROFILE("AUTOFILL_PROFILE"),
    /**
     * A bookmark folder or a bookmark URL object.
     */
    BOOKMARK("BOOKMARK"),
    /**
     * Flags to enable experimental features.
     */
    EXPERIMENTS("EXPERIMENTS"),
    /**
     * An object representing a set of Nigori keys.
     */
    NIGORI("NIGORI"),
    /**
     * A password entry.
     */
    PASSWORD("PASSWORD"),
    /**
     * An object representing a preference.
     */
    PREFERENCE("PREFERENCE"),
    /**
     * An object representing a priority preference.
     */
    PRIORITY_PREFERENCE("PRIORITY_PREFERENCE"),
    /**
     * An object representing a browser session or tab.
     */
    SESSION("SESSION"),
    /**
     * A typed_url folder or a typed_url object.
     */
    TYPED_URL("TYPED_URL"),
    /**
     * A history delete directive object.
     */
    HISTORY_DELETE_DIRECTIVE("HISTORY_DELETE_DIRECTIVE"),
    /**
     * A device info object.
     */
    DEVICE_INFO("DEVICE_INFO"),
    /**
     * A proxy tabs object (placeholder for sessions).
     */
    PROXY_TABS("NULL", true),
    /**
     * A favicon image object.
     */
    FAVICON_IMAGE("FAVICON_IMAGE"),
    /**
     * A favicon tracking object.
     */
    FAVICON_TRACKING("FAVICON_TRACKING"),
    /**
     * A supervised user setting object. The old name "managed user" is used for backwards
     * compatibility.
     */
    MANAGED_USER_SETTING("MANAGED_USER_SETTING"),
    /**
     * A supervised user whitelist object.
     */
    MANAGED_USER_WHITELIST("MANAGED_USER_WHITELIST"),
    /**
     * An autofill wallet data object.
     */
    AUTOFILL_WALLET("AUTOFILL_WALLET"),
    /**
     * Usage counts and last use dates for autofill wallet data objects.
     */
    AUTOFILL_WALLET_METADATA("AUTOFILL_WALLET_METADATA");

    private static final String TAG = "ModelType";

    private final String mModelType;

    private final boolean mNonInvalidationType;

    ModelType(String modelType, boolean nonInvalidationType) {
        assert nonInvalidationType || name().equals(modelType);
        mModelType = modelType;
        mNonInvalidationType = nonInvalidationType;
    }

    ModelType(String modelType) {
        this(modelType, false);
    }

    private boolean isNonInvalidationType() {
        return mNonInvalidationType;
    }

    /**
     * Returns the {@link ObjectId} representation of this {@link ModelType}.
     *
     * This should be used with caution, since it converts even {@link ModelType} instances with
     * |mNonInvalidationType| set. For automatically stripping such {@link ModelType} entries out,
     * use {@link ModelType#modelTypesToObjectIds(java.util.Set)} instead.
     */
    @VisibleForTesting
    public ObjectId toObjectId() {
        return ObjectId.newInstance(Types.ObjectSource.CHROME_SYNC, mModelType.getBytes());
    }

    public static ModelType fromObjectId(ObjectId objectId) {
        try {
            return valueOf(new String(objectId.getName()));
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    /**
     * Converts string representations of types to sync to {@link ModelType}s.
     * Returns set of {@code ModelType} values for all elements of {@code syncTypes} for which
     * {@link ModelType#valueOf(String)} successfully returns; other elements are dropped.
     */
    public static Set<ModelType> syncTypesToModelTypes(Collection<String> syncTypes) {
        Set<ModelType> modelTypes = new HashSet<ModelType>(syncTypes.size());
        for (String syncType : syncTypes) {
            try {
                modelTypes.add(valueOf(syncType));
            } catch (IllegalArgumentException exception) {
                // Drop invalid sync types.
                Log.w(TAG, "Could not translate sync type to model type: " + syncType);
            }
        }
        return modelTypes;
    }

    /**
     * Converts a set of sync types {@link String} to a set of {@link ObjectId}.
     *
     * This strips out any {@link ModelType} that is not an invalidation type.
     */
    public static Set<ObjectId> syncTypesToObjectIds(Collection<String> syncTypes) {
        return modelTypesToObjectIds(syncTypesToModelTypes(syncTypes));
    }

    /**
     * Converts a set of {@link ModelType} to a set of {@link ObjectId}.
     *
     * This strips out any {@link ModelType} that is not an invalidation type.
     */
    public static Set<ObjectId> modelTypesToObjectIds(Set<ModelType> modelTypes) {
        Set<ModelType> filteredModelTypes = filterOutNonInvalidationTypes(modelTypes);
        Set<ObjectId> objectIds = new HashSet<ObjectId>(filteredModelTypes.size());
        for (ModelType modelType : filteredModelTypes) {
            objectIds.add(modelType.toObjectId());
        }
        return objectIds;
    }

    /** Converts a set of {@link ModelType} to a set of string names. */
    @VisibleForTesting
    public static Set<String> modelTypesToSyncTypesForTest(Set<ModelType> modelTypes) {
        Set<String> objectIds = new HashSet<String>(modelTypes.size());
        for (ModelType modelType : modelTypes) {
            objectIds.add(modelType.toString());
        }
        return objectIds;
    }

    /** Filters out non-invalidation types from a set of {@link ModelType}. */
    @VisibleForTesting
    public static Set<ModelType> filterOutNonInvalidationTypes(Set<ModelType> modelTypes) {
        Set<ModelType> filteredTypes = new HashSet<ModelType>(modelTypes.size());
        for (ModelType modelType : modelTypes) {
            if (!modelType.isNonInvalidationType()) {
                filteredTypes.add(modelType);
            }
        }
        return filteredTypes;
    }

}
