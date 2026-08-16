package com.nexus.adbwatchdog;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

/**
 * Status UI. Watchdog Service auto-starts on app launch and runs permanently.
 */
public class MainActivity extends AppCompatActivity {

    private TextView tvRoot;
    private TextView tvStatus;
    private TextView tvLog;
    private final Handler uiHandler = new Handler();
    private final Runnable refreshTask = new Runnable() {
        @Override
        public void run() {
            refreshViews();
            uiHandler.postDelayed(this, 3000);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvRoot = findViewById(R.id.tvRoot);
        tvStatus = findViewById(R.id.tvStatus);
        tvLog = findViewById(R.id.tvLog);

        Button btnStart = findViewById(R.id.btnStart);
        Button btnStop = findViewById(R.id.btnStop);
        Button btnRefresh = findViewById(R.id.btnRefresh);
        Button btnStopAdbd = findViewById(R.id.btnTestStopAdbd);
        Button btnBreakPort = findViewById(R.id.btnTestBreakPort);

        // Permanent mode: Start is always available (idempotent); Stop is disabled.
        btnStart.setText("Running");
        btnStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                NexusADBWatchdogService.start(MainActivity.this);
                Toast.makeText(MainActivity.this,
                        "Watchdog already set to auto-run permanently", Toast.LENGTH_SHORT).show();
                refreshViews();
            }
        });
        btnStop.setText("No Stop");
        btnStop.setEnabled(false);
        btnStop.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Toast.makeText(MainActivity.this,
                        "Stop disabled — Watchdog runs permanently", Toast.LENGTH_LONG).show();
            }
        });
        btnRefresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                refreshViews();
            }
        });

        // DEVELOPMENT / TEST ONLY
        btnStopAdbd.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent i = new Intent(MainActivity.this, NexusADBWatchdogService.class);
                i.setAction(WatchdogConfig.ACTION_INJECT);
                i.putExtra(WatchdogConfig.EXTRA_INJECT, "STOP_ADBD");
                startService(i);
                NexusADBWatchdogService.start(MainActivity.this);
                Toast.makeText(MainActivity.this,
                        "TEST ONLY: STOP_ADBD inject queued", Toast.LENGTH_LONG).show();
            }
        });
        btnBreakPort.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent i = new Intent(MainActivity.this, NexusADBWatchdogService.class);
                i.setAction(WatchdogConfig.ACTION_INJECT);
                i.putExtra(WatchdogConfig.EXTRA_INJECT, "BREAK_PORT");
                startService(i);
                NexusADBWatchdogService.start(MainActivity.this);
                Toast.makeText(MainActivity.this,
                        "TEST ONLY: BREAK_PORT inject queued", Toast.LENGTH_LONG).show();
            }
        });

        StatusStore.ensurePublicFolder(this);
        // Auto-start immediately on open; Service ignores Stop.
        NexusADBWatchdogService.start(this);
        refreshViews();
    }

    @Override
    protected void onResume() {
        super.onResume();
        NexusADBWatchdogService.start(this);
        uiHandler.post(refreshTask);
    }

    @Override
    protected void onPause() {
        uiHandler.removeCallbacks(refreshTask);
        // Do NOT stop Service when leaving UI.
        super.onPause();
    }

    private void refreshViews() {
        boolean root = RootShell.hasRoot();
        String logPath = StatusStore.logFile(this).getAbsolutePath();
        String statusPath = StatusStore.statusFile(this).getAbsolutePath();
        tvRoot.setText("ROOT=" + (root ? "YES" : "NO (needs su)")
                + "\nMODE=PERMANENT AUTO-START"
                + "\nLOG folder:\n" + StatusStore.PUBLIC_DIR_PATH
                + "\nstatus=" + statusPath
                + "\nlog=" + logPath);
        String status = StatusStore.readStatus(this);
        tvStatus.setText(status.isEmpty()
                ? "(waiting for first status write…)"
                : status);
        String log = StatusStore.readLogTail(this, 60);
        tvLog.setText(log.isEmpty() ? "(no log yet — wait a few seconds)" : log);
    }
}
