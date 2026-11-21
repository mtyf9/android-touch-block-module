package local.androidtouchblock.notice;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.view.Gravity;
import android.view.WindowManager;
import android.widget.TextView;

public final class WindowNotice extends Activity {
    private final Handler handler = new Handler();
    private final Runnable close = this::finish;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED
                | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE);
        getWindow().setGravity(Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        getWindow().setLayout(WindowManager.LayoutParams.WRAP_CONTENT, WindowManager.LayoutParams.WRAP_CONTENT);
        int pad = Math.round(12 * getResources().getDisplayMetrics().density);
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.y = 2 * pad;
        getWindow().setAttributes(params);
        TextView message = new TextView(this);
        message.setText("触摸已临时恢复 30 秒，按电源键可提前结束");
        message.setTextColor(Color.WHITE);
        message.setTextSize(16);
        message.setPadding(pad, pad, pad, pad);
        GradientDrawable background = new GradientDrawable();
        background.setColor(0xff303030);
        background.setCornerRadius(pad);
        message.setBackground(background);
        setContentView(message);
        handler.postDelayed(close, 2200);
    }

    @Override protected void onDestroy() {
        handler.removeCallbacks(close);
        super.onDestroy();
    }
}
