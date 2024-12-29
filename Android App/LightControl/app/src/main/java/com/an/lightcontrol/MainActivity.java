package com.an.lightcontrol;

import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.widget.TextView;
import android.widget.ToggleButton;
import androidx.appcompat.app.AppCompatActivity;
import okhttp3.*;
import okhttp3.logging.HttpLoggingInterceptor;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.IOException;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TableLayout;
import android.widget.TableRow;
import android.widget.Toast;
import android.util.Log;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "ESP8266Control";
    private static final String BASE_URL = "http://192.168.29.17";
    private OkHttpClient client;
    private TextView statusText;
    private TextView timeText;
    private ToggleButton[] relayButtons = new ToggleButton[4];
    private final Handler handler = new Handler();
    private static final int UPDATE_INTERVAL = 1000; // 1 second
    private static final String TAG2 = "MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Setup logging interceptor
        HttpLoggingInterceptor logging = new HttpLoggingInterceptor(message ->
                Log.d(TAG, message));
        logging.setLevel(HttpLoggingInterceptor.Level.BODY);

        client = new OkHttpClient.Builder()
                .addInterceptor(logging)
                .build();

        statusText = findViewById(R.id.statusText);
        timeText = findViewById(R.id.timeText);

        // Initialize buttons
        relayButtons[0] = findViewById(R.id.relay1Button);
        relayButtons[1] = findViewById(R.id.relay2Button);
        relayButtons[2] = findViewById(R.id.relay3Button);
        relayButtons[3] = findViewById(R.id.relay4Button);
        Button addScheduleButton = findViewById(R.id.addScheduleButton);
        Spinner relaySpinner = findViewById(R.id.relaySelect);
        EditText onTimeInput = findViewById(R.id.onTime);
        EditText offTimeInput = findViewById(R.id.offTime);

        addScheduleButton.setOnClickListener(v -> {
            int relay = Integer.parseInt(relaySpinner.getSelectedItem().toString().split(" ")[1]);
            String onTime = onTimeInput.getText().toString();
            String offTime = offTimeInput.getText().toString();
            if (!onTime.isEmpty() && !offTime.isEmpty()) {
                addSchedule(relay, onTime, offTime);
            } else {
                Toast.makeText(MainActivity.this, "Please enter valid times", Toast.LENGTH_SHORT).show();
            }
        });

        for (int i = 0; i < relayButtons.length; i++) {
            final int relayNumber = i + 1;
            relayButtons[i].setOnCheckedChangeListener((buttonView, isChecked) -> {
                if (buttonView.isPressed()) {
                    toggleRelay(relayNumber);
                }
            });
        }

        // Immediately get the current states on launch
        updateRelayStates();

        // Start periodic updates
        startPeriodicUpdates();
        fetchSchedules();
    }

    private void startPeriodicUpdates() {
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                updateTime();
                updateRelayStates();
                handler.postDelayed(this, UPDATE_INTERVAL);
            }
        }, UPDATE_INTERVAL);
    }

    private void updateTime() {
        Request request = new Request.Builder()
                .url(BASE_URL + "/time")
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                Log.e(TAG, "Time update failed: " + e.getMessage());
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                if (response.isSuccessful()) {
                    final String time = response.body().string();
                    runOnUiThread(() -> timeText.setText("Time: " + time));
                }
            }
        });
    }

    private void updateRelayStates() {
        Request request = new Request.Builder()
                .url(BASE_URL + "/relay/status")
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                Log.e(TAG, "Status update failed: " + e.getMessage());
                runOnUiThread(() -> statusText.setText("Failed to update relay states"));
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                if (response.isSuccessful()) {
                    String responseBody = response.body().string();
                    Log.d(TAG, "Relay Status Response: " + responseBody);
                    try {
                        JSONObject states = new JSONObject(responseBody);
                        runOnUiThread(() -> {
                            try {
                                for (int i = 0; i < relayButtons.length; i++) {
                                    // Parse relay state as integer and convert to boolean
                                    int stateInt = states.optInt(String.valueOf(i + 1), 0); // Default to 0 if key not found
                                    boolean state = (stateInt == 1);
                                    relayButtons[i].setChecked(state);
                                    Log.d(TAG, "Relay " + (i + 1) + " state: " + state);
                                }
                                statusText.setText("Relay states updated");
                            } catch (Exception e) {
                                Log.e(TAG, "Error parsing relay states", e);
                                statusText.setText("Error parsing relay states");
                            }
                        });
                    } catch (Exception e) {
                        Log.e(TAG, "Error parsing response", e);
                        runOnUiThread(() -> statusText.setText("Error parsing response"));
                    }
                } else {
                    Log.e(TAG, "Unsuccessful response: " + response.code());
                    runOnUiThread(() -> statusText.setText("Error: " + response.code()));
                }
            }
        });
    }

    private void toggleRelay(int relayNumber) {
        Request request = new Request.Builder()
                .url(BASE_URL + "/relay/" + relayNumber)
                .post(RequestBody.create(null, new byte[0]))
                .build();

        relayButtons[relayNumber-1].setEnabled(false);

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                Log.e(TAG, "Network error: " + e.getMessage(), e);
                runOnUiThread(() -> {
                    statusText.setText("Connection Error: " + e.getMessage());
                    relayButtons[relayNumber-1].setEnabled(true);
                    updateRelayStates(); // Refresh states
                });
            }

            @Override
            public void onResponse(Call call, Response response) {
                runOnUiThread(() -> {
                    relayButtons[relayNumber-1].setEnabled(true);
                    if (response.isSuccessful()) {
                        statusText.setText("Relay " + relayNumber + " toggled");
                    } else {
                        statusText.setText("Error: " + response.code());
                        updateRelayStates(); // Refresh states
                    }
                });
            }
        });
    }

    private void fetchSchedules() {
        Request request = new Request.Builder()
                .url(BASE_URL + "/schedules")
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                runOnUiThread(() -> Toast.makeText(MainActivity.this, "Failed to fetch schedules", Toast.LENGTH_SHORT).show());
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                if (response.isSuccessful()) {
                    String responseBody = response.body().string();
                    runOnUiThread(() -> updateScheduleTable(responseBody));
                } else {
                    runOnUiThread(() -> Toast.makeText(MainActivity.this, "Error fetching schedules", Toast.LENGTH_SHORT).show());
                }
            }
        });
    }

    private void addSchedule(int relay, String onTime, String offTime) {
        JSONObject json = new JSONObject();
        try {
            json.put("relay", relay);
            json.put("onTime", onTime);
            json.put("offTime", offTime);
        } catch (Exception e) {
            e.printStackTrace();
        }

        RequestBody body = RequestBody.create(json.toString(), MediaType.parse("application/json"));

        Request request = new Request.Builder()
                .url(BASE_URL + "/schedule/add")
                .post(body)
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                runOnUiThread(() -> Toast.makeText(MainActivity.this, "Failed to add schedule", Toast.LENGTH_SHORT).show());
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                if (response.isSuccessful()) {
                    runOnUiThread(() -> {
                        Toast.makeText(MainActivity.this, "Schedule added", Toast.LENGTH_SHORT).show();
                        fetchSchedules();
                    });
                } else {
                    runOnUiThread(() -> Toast.makeText(MainActivity.this, "Error adding schedule", Toast.LENGTH_SHORT).show());
                }
            }
        });
    }

    private void deleteSchedule(int id) {
        HttpUrl url = HttpUrl.parse(BASE_URL + "/schedule/delete").newBuilder()
                .addQueryParameter("id", String.valueOf(id))
                .build();

        Request request = new Request.Builder()
                .url(url)
                .delete()
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                runOnUiThread(() -> Toast.makeText(MainActivity.this, "Failed to delete schedule", Toast.LENGTH_SHORT).show());
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                if (response.isSuccessful()) {
                    runOnUiThread(() -> {
                        Toast.makeText(MainActivity.this, "Schedule deleted", Toast.LENGTH_SHORT).show();
                        fetchSchedules();
                    });
                } else {
                    runOnUiThread(() -> Toast.makeText(MainActivity.this, "Error deleting schedule", Toast.LENGTH_SHORT).show());
                }
            }
        });
    }

    private void updateScheduleTable(String json) {
        Log.d(TAG2, "Received JSON: " + json); // Log the JSON response
        try {
            JSONArray schedules = new JSONArray(json);
            TableLayout table = findViewById(R.id.scheduleTable);

            // Remove existing rows except the header
            if (table.getChildCount() > 1) {
                table.removeViews(1, table.getChildCount() - 1);
            }

            for (int i = 0; i < schedules.length(); i++) {
                JSONObject schedule = schedules.getJSONObject(i);
                int id = schedule.getInt("id");
                int relay = schedule.getInt("relay");
                int onHour = schedule.getInt("onHour");
                int onMinute = schedule.getInt("onMinute");
                int offHour = schedule.getInt("offHour");
                int offMinute = schedule.getInt("offMinute");
                boolean enabled = schedule.getBoolean("enabled");

                TableRow row = new TableRow(this);

                TextView relayView = new TextView(this);
                relayView.setText("Relay " + relay);
                row.addView(relayView);

                TextView onTimeView = new TextView(this);
                onTimeView.setText(String.format("%02d:%02d", onHour, onMinute));
                row.addView(onTimeView);

                TextView offTimeView = new TextView(this);
                offTimeView.setText(String.format("%02d:%02d", offHour, offMinute));
                row.addView(offTimeView);

                TextView statusView = new TextView(this);
                statusView.setText(enabled ? "Active" : "Inactive");
                row.addView(statusView);

                Button deleteButton = new Button(this);
                deleteButton.setText("Delete");
                int scheduleId = id;
                deleteButton.setOnClickListener(v -> deleteSchedule(scheduleId));
                row.addView(deleteButton);

                table.addView(row);
            }
        } catch (JSONException e) {
            Log.e(TAG2, "JSON Parsing Error: " + e.getMessage());
            runOnUiThread(() -> Toast.makeText(MainActivity.this, "Error parsing schedules", Toast.LENGTH_SHORT).show());
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        handler.removeCallbacksAndMessages(null);
    }
}