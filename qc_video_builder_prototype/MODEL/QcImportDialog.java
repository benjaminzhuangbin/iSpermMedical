package com.isperm.qc;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * iSperm Medical QC Video 导入交互控制器 (UI 对话框组件)
 * ===================================================
 * 在主控 APP 的 QC 操作界面中，当用户点击【Import Video】按键时直接调用此类：
 * 
 * 示例用法（在 QC Activity / Fragment 中的按键监听）：
 * <pre>
 *   btnImportVideo.setOnClickListener(new View.OnClickListener() {
 *       @Override
 *       public void onClick(View v) {
 *           QcImportDialog.showImportDialog(MainActivity.this, new QcImportDialog.OnImportCompletedListener() {
 *               @Override
 *               public void onImportFinished(QcVideoImporter.ImportResult result) {
 *                   // 导入完成，刷新界面上 QC 视频列表或开始分析
 *                   refreshQcVideoList();
 *               }
 *           });
 *       }
 *   });
 * </pre>
 */
public class QcImportDialog {

    private static final String TAG = "QcImportDialog";

    /**
     * 导入完成对外回调
     */
    public interface OnImportCompletedListener {
        void onImportFinished(QcVideoImporter.ImportResult result);
    }

    /**
     * 弹出导入视频主选择框
     * 
     * @param activity 当前前台 Activity
     * @param listener 导入完成监听器
     */
    public static void showImportDialog(final Activity activity, final OnImportCompletedListener listener) {
        if (activity == null || activity.isFinishing()) {
            return;
        }

        // 1. 自动扫描所有 USB 挂载路径中的 QC-Video 文件夹
        final List<File> availableVideoFolders = scanUsbForQcVideos();

        if (availableVideoFolders.isEmpty()) {
            new AlertDialog.Builder(activity)
                    .setTitle("未检测到 QC Video U 盘")
                    .setMessage("请确认：\n" +
                            "1. 专属 QC U 盘已牢固插入分析仪 USB 接口。\n" +
                            "2. U 盘根目录下已包含 QC-Video 文件夹及各视频子目录 (如 Video-1, Video-2)。\n\n" +
                            "若刚插入，请等待 2~3 秒待系统挂载后再试。")
                    .setPositiveButton("重试扫描", new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            showImportDialog(activity, listener);
                        }
                    })
                    .setNegativeButton("取消", null)
                    .show();
            return;
        }

        // 2. 构建视频选择列表
        final String[] folderNames = new String[availableVideoFolders.size()];
        final File internalBase = new File(QcConstants.DEFAULT_INTERNAL_QC_PATH);

        for (int i = 0; i < availableVideoFolders.size(); i++) {
            File f = availableVideoFolders.get(i);
            String name = f.getName();
            File alreadyImported = new File(internalBase, name);
            if (alreadyImported.exists() && alreadyImported.isDirectory()) {
                folderNames[i] = name + "  [仪器内已存在，点击可重新覆盖]";
            } else {
                folderNames[i] = name + "  [待载入]";
            }
        }

        // 3. 弹出单选列表对话框
        new AlertDialog.Builder(activity)
                .setTitle("选择要载入的 QC 质控视频")
                .setItems(folderNames, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, final int which) {
                        File selectedVideoFolder = availableVideoFolders.get(which);
                        executeImportWithProgress(activity, selectedVideoFolder, listener);
                    }
                })
                .setNegativeButton("取消", null)
                .show();
    }

    /**
     * 执行导入并展示进度弹窗
     */
    private static void executeImportWithProgress(final Activity activity,
                                                 final File videoFolder,
                                                 final OnImportCompletedListener listener) {
        final ProgressDialog progressDialog = new ProgressDialog(activity);
        progressDialog.setTitle("正在载入 QC 视频");
        progressDialog.setMessage("正在验证授权证书与数字签名...");
        progressDialog.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
        progressDialog.setMax(100);
        progressDialog.setCancelable(false);
        progressDialog.setCanceledOnTouchOutside(false);
        progressDialog.show();

        QcVideoImporter.importVideoAsync(videoFolder, null, new QcVideoImporter.ImportCallback() {
            @Override
            public void onProgress(String statusMessage, int progressPercent) {
                if (activity.isFinishing()) return;
                progressDialog.setMessage(statusMessage);
                progressDialog.setProgress(progressPercent);
            }

            @Override
            public void onSuccess(final QcVideoImporter.ImportResult result) {
                if (activity.isFinishing()) return;
                progressDialog.dismiss();

                // 弹出导入成功弹窗，并提供“继续导入下一个视频”选项
                new AlertDialog.Builder(activity)
                        .setTitle("✓ 载入成功")
                        .setMessage("视频 [" + result.videoName + "] 已成功解密并导入！\n\n" +
                                "• 帧数: " + result.frameCount + " 张标准图片\n" +
                                "• 内部存储路径: " + result.outputDirectory.getAbsolutePath() + "\n" +
                                "• 耗时: " + result.elapsedTimeMs + " 毫秒\n\n" +
                                "仪器系统已可以正常加载和播放本视频。")
                        .setPositiveButton("继续载入其他视频", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                if (listener != null) {
                                    listener.onImportFinished(result);
                                }
                                // 重复操作，便于用户连续导入 Video-2, Video-3 等
                                showImportDialog(activity, listener);
                            }
                        })
                        .setNegativeButton("完成", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                if (listener != null) {
                                    listener.onImportFinished(result);
                                }
                            }
                        })
                        .setCancelable(false)
                        .show();
            }

            @Override
            public void onError(String errorMessage, Throwable throwable) {
                if (activity.isFinishing()) return;
                progressDialog.dismiss();

                new AlertDialog.Builder(activity)
                        .setTitle("❌ 载入失败")
                        .setMessage("视频未能导入成功：\n" + errorMessage)
                        .setPositiveButton("确定", null)
                        .show();
            }
        });
    }

    /**
     * 自动扫描系统各种可能挂载的 USB 目录中的 QC-Video/Video-* 文件夹
     */
    private static List<File> scanUsbForQcVideos() {
        List<File> result = new ArrayList<>();

        for (String mountPath : QcConstants.USB_CANDIDATE_PATHS) {
            File mountDir = new File(mountPath);
            if (!mountDir.exists() || !mountDir.canRead()) {
                continue;
            }

            // 1. 直接挂载目录下是否有 QC-Video 文件夹
            File directQcDir = new File(mountDir, QcConstants.QC_VIDEO_FOLDER_NAME);
            if (directQcDir.exists() && directQcDir.isDirectory()) {
                collectVideoSubfolders(directQcDir, result);
            }

            // 2. 检查多级子挂载卷 (例如 /mnt/usb_storage/USB_DISK0/QC-Video)
            File[] subMounted = mountDir.listFiles();
            if (subMounted != null) {
                for (File sub : subMounted) {
                    if (sub.isDirectory()) {
                        File qcDir = new File(sub, QcConstants.QC_VIDEO_FOLDER_NAME);
                        if (qcDir.exists() && qcDir.isDirectory()) {
                            collectVideoSubfolders(qcDir, result);
                        }
                    }
                }
            }
        }

        // 排序：按 Video-1, Video-2 自然顺序排列
        Collections.sort(result, new Comparator<File>() {
            @Override
            public int compare(File o1, File o2) {
                return o1.getName().compareToIgnoreCase(o2.getName());
            }
        });

        return result;
    }

    /**
     * 收集目录下所有 Video- 开头的文件夹
     */
    private static void collectVideoSubfolders(File qcFolder, List<File> list) {
        File[] files = qcFolder.listFiles();
        if (files == null) return;
        for (File f : files) {
            if (f.isDirectory()) {
                String name = f.getName().toLowerCase();
                if (name.startsWith("video-") || name.startsWith("video_")) {
                    if (!list.contains(f)) {
                        list.add(f);
                    }
                }
            }
        }
    }
}
