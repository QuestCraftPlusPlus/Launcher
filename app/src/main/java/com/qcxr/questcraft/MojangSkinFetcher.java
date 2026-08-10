package com.qcxr.questcraft;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import java.io.*;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MojangSkinFetcher {

    private static final String PROFILES_URL = "https://api.mojang.com/profiles/minecraft";
    private static final String SESSION_URL = "https://sessionserver.mojang.com/session/minecraft/profile/";

    private static final ExecutorService EXECUTOR = Executors.newCachedThreadPool();

    public record SkinResult(byte[] skinPngBytes, boolean slim) { }

    private record SkinMeta(String url, boolean slim) { }

    public static CompletableFuture<SkinResult> fetchSkin(String username) {
        return CompletableFuture.supplyAsync(() -> fetchSkinBlocking(username), EXECUTOR);
    }

    private static SkinResult fetchSkinBlocking(String username) {
        try {
            String uuid = resolveUuid(username);
            String texturesBase64 = fetchTexturesProperty(uuid);
            SkinMeta meta = parseSkinUrlAndSlim(texturesBase64);
            byte[] pngBytes = fetchSkinPngBytes(meta.url());
            return new SkinResult(pngBytes, meta.slim());
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    private static String resolveUuid(String username) throws IOException {
        JsonArray requestBody = new JsonArray();
        requestBody.add(username);

        HttpURLConnection conn = (HttpURLConnection) new URL(PROFILES_URL).openConnection();
        conn.setRequestMethod("POST");
        conn.setRequestProperty("Content-Type", "application/json");
        conn.setDoOutput(true);

        try (OutputStream os = conn.getOutputStream()) {
            os.write(requestBody.toString().getBytes(StandardCharsets.UTF_8));
        }

        String body = readBody(conn);
        JsonElement parsed = JsonParser.parseString(body);

        if (!(parsed instanceof JsonArray arr) || arr.isEmpty()) {
            throw new IOException("No matching Mojang profile for username: " + username);
        }

        JsonObject first = arr.get(0).getAsJsonObject();
        return first.get("id").getAsString();
    }

    private static String fetchTexturesProperty(String uuid) throws IOException {
        HttpURLConnection conn = (HttpURLConnection) new URL(SESSION_URL + uuid).openConnection();
        conn.setRequestMethod("GET");

        String body = readBody(conn);
        JsonObject profile = JsonParser.parseString(body).getAsJsonObject();
        JsonArray properties = profile.getAsJsonArray("properties");

        for (JsonElement el : properties) {
            JsonObject prop = el.getAsJsonObject();
            if ("textures".equals(prop.get("name").getAsString())) {
                return prop.get("value").getAsString();
            }
        }

        throw new IOException("No 'textures' property found for UUID: " + uuid);
    }

    private static SkinMeta parseSkinUrlAndSlim(String texturesBase64) {
        byte[] decoded = Base64.getDecoder().decode(texturesBase64);
        String json = new String(decoded, StandardCharsets.UTF_8);

        JsonObject root = JsonParser.parseString(json).getAsJsonObject();
        JsonObject skin = root.getAsJsonObject("textures").getAsJsonObject("SKIN");
        String url = skin.get("url").getAsString();

        boolean slim = skin.has("metadata")
                && skin.getAsJsonObject("metadata").has("model")
                && skin.getAsJsonObject("metadata").get("model").getAsString().equals("slim");

        return new SkinMeta(url, slim);
    }

    private static byte[] fetchSkinPngBytes(String skinUrl) throws IOException {
        String httpsUrl = skinUrl.startsWith("http://")
                ? "https://" + skinUrl.substring("http://".length())
                : skinUrl;

        HttpURLConnection conn = (HttpURLConnection) new URL(httpsUrl).openConnection();
        conn.setRequestMethod("GET");
        return readBytes(conn);
    }

    private static boolean isSuccess(int status) {
        return status / 100 == 2;
    }

    private static String readBody(HttpURLConnection conn) throws IOException {
        int status = conn.getResponseCode();
        InputStream stream = isSuccess(status) ? conn.getInputStream() : conn.getErrorStream();

        if (stream == null) {
            throw new IOException("HTTP " + status + " with no response body from " + conn.getURL());
        }

        StringBuilder sb = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(stream, StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line);
            }
        } finally {
            conn.disconnect();
        }

        if (!isSuccess(status)) {
            throw new IOException("HTTP " + status + " from " + conn.getURL() + ": " + sb);
        }

        return sb.toString();
    }

    private static byte[] readBytes(HttpURLConnection conn) throws IOException {
        int status = conn.getResponseCode();
        InputStream stream = isSuccess(status) ? conn.getInputStream() : conn.getErrorStream();

        if (stream == null) {
            throw new IOException("HTTP " + status + " with no response body from " + conn.getURL());
        }

        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        try {
            byte[] chunk = new byte[8192];
            int read;
            while ((read = stream.read(chunk)) != -1) {
                buffer.write(chunk, 0, read);
            }
        } finally {
            stream.close();
            conn.disconnect();
        }

        if (!isSuccess(status)) {
            throw new IOException("HTTP " + status + " fetching PNG from " + conn.getURL());
        }

        return buffer.toByteArray();
    }
}
