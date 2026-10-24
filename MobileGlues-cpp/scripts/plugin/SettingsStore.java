// SPDX-License-Identifier: LGPL-2.1-only
package com.fcl.plugin.mobileglues.mc26test;

import android.util.AtomicFile;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/** The same numeric JSON format consumed by config/settings.cpp. */
public final class SettingsStore {
    private final File directory;
    private final AtomicFile config;

    public SettingsStore(File directory) {
        this.directory = directory;
        config = new AtomicFile(new File(directory, "config.json"));
    }

    public static JSONObject defaults() throws JSONException {
        return new JSONObject().put("enableANGLE", 1).put("enableNoError", 0)
                .put("enableExtComputeShader", 0).put("enableExtTimerQuery", 1)
                .put("enableExtDirectStateAccess", 0).put("maxGlslCacheSize", 32)
                .put("angleDepthClearFixMode", 0).put("customGLVersion", 40)
                .put("fsr1Setting", 0).put("hideMGEnvLevel", 0);
    }

    public synchronized JSONObject loadOrCreate() throws IOException, JSONException {
        File file = config.getBaseFile();
        if (!file.exists() && !new File(file.getPath() + ".bak").exists()) {
            JSONObject value = defaults();
            save(value);
            return value;
        }
        try (FileInputStream input = config.openRead();
             ByteArrayOutputStream bytes = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[4096];
            int read;
            while ((read = input.read(buffer)) != -1) {
                if (bytes.size() + read > 1024 * 1024)
                    throw new IOException("The settings file is larger than 1 MiB.");
                bytes.write(buffer, 0, read);
            }
            return new JSONObject(new String(bytes.toByteArray(), StandardCharsets.UTF_8));
        }
    }

    public synchronized void save(JSONObject value) throws IOException, JSONException {
        validate(value);
        if (!directory.isDirectory() && !directory.mkdirs())
            throw new IOException("Could not create the renderer settings folder.");
        byte[] bytes = value.toString(2).getBytes(StandardCharsets.UTF_8);
        FileOutputStream output = null;
        try {
            output = config.startWrite();
            output.write(bytes);
            config.finishWrite(output);
        } catch (IOException error) {
            if (output != null) config.failWrite(output);
            throw error;
        }
    }

    public synchronized JSONObject reset() throws IOException, JSONException {
        File existing = config.getBaseFile();
        if (existing.isFile())
            Files.copy(existing.toPath(), new File(directory, "config.before-reset.json").toPath(),
                    StandardCopyOption.REPLACE_EXISTING);
        JSONObject value = defaults();
        save(value);
        return value;
    }

    public synchronized long cacheBytes() {
        return new File(directory, "glsl_cache.tmp").length();
    }

    public synchronized void clearShaderCache() throws IOException {
        for (String name : new String[]{"glsl_cache.tmp", "glsl_cache.tmp.new"}) {
            File file = new File(directory, name);
            if (file.exists() && !file.delete())
                throw new IOException("Could not clear the shader cache.");
        }
    }

    public static int number(JSONObject value, String key, int fallback) {
        Object item = value.opt(key);
        return item instanceof Number ? ((Number) item).intValue() : fallback;
    }

    private static void validate(JSONObject value) throws JSONException {
        range(value, "enableANGLE", 0, 3);
        range(value, "enableNoError", 0, 3);
        range(value, "enableExtComputeShader", 0, 1);
        range(value, "enableExtTimerQuery", 0, 1);
        range(value, "enableExtDirectStateAccess", 0, 1);
        range(value, "maxGlslCacheSize", 0, 1024);
        range(value, "angleDepthClearFixMode", 0, 2);
        range(value, "fsr1Setting", 0, 4);
        range(value, "hideMGEnvLevel", 0, 1);
        if (value.has("customGLVersion")) {
            int gl = number(value, "customGLVersion", -1);
            if (gl != 0 && gl != 32 && gl != 33 && (gl < 40 || gl > 46))
                throw new JSONException("Choose a supported OpenGL version.");
        }
    }

    private static void range(JSONObject value, String key, int min, int max) throws JSONException {
        if (!value.has(key)) return;
        Object raw = value.get(key);
        if (!(raw instanceof Number) || ((Number) raw).doubleValue() != ((Number) raw).intValue()
                || ((Number) raw).intValue() < min || ((Number) raw).intValue() > max)
            throw new JSONException("Invalid value for " + key + ".");
    }
}
