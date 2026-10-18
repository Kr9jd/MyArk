#include "disassemblydialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QFont>

#include <capstone/capstone.h>

static QString hex(ULONG64 v)
{
    return QString("0x%1").arg(v, 16, 16, QLatin1Char('0'));
}

DisassemblyDialog::DisassemblyDialog(KernelDevice* device, ULONG64 address,
    const QString& title, QWidget* parent)
    : QDialog(parent)
    , m_device(device)
    , m_address(address)
{
    setWindowTitle(QString("Disassembly - %1").arg(title));
    resize(900, 560);

    auto* root = new QVBoxLayout(this);

    m_infoLabel = new QLabel(QString("Address: %1").arg(hex(address)), this);
    root->addWidget(m_infoLabel);

    auto* top = new QHBoxLayout();
    auto* refreshBtn = new QPushButton("Refresh", this);
    top->addWidget(refreshBtn);
    top->addStretch();
    root->addLayout(top);

    m_text = new QTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QTextEdit::NoWrap);
    QFont monoFont("Consolas", 10);
    monoFont.setStyleHint(QFont::Monospace);
    m_text->setFont(monoFont);
    root->addWidget(m_text);

    connect(refreshBtn, &QPushButton::clicked, this, &DisassemblyDialog::refresh);
    refresh();
}

void DisassemblyDialog::refresh()
{
    m_text->clear();

    const ULONG64 kReadSize = 0x200;
    auto bytes = m_device->readMemory(m_address, kReadSize);
    if (bytes.empty())
    {
        m_infoLabel->setText(QString("Address: %1  (read failed)").arg(hex(m_address)));
        m_text->setHtml(
            "<span style=\"color:#d9534f;\">Failed to read kernel memory.</span>");
        return;
    }

    csh handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
    {
        m_infoLabel->setText("Capstone init failed");
        m_text->setHtml(
            "<span style=\"color:#d9534f;\">Capstone initialization failed.</span>");
        return;
    }
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_OFF);

    cs_insn* insn = nullptr;
    size_t count = cs_disasm(handle, bytes.data(), bytes.size(), m_address, 0, &insn);

    QString html;
    html += "<pre style=\"font-family:Consolas,'Courier New',monospace; margin:0;\">";

    for (size_t i = 0; i < count; ++i)
    {
        QString addr = hex(insn[i].address);

        QString byteStr;
        for (int b = 0; b < insn[i].size; ++b)
            byteStr += QString("%1 ").arg(insn[i].bytes[b], 2, 16, QLatin1Char('0')).toUpper();
        byteStr = byteStr.trimmed();
        // 字节列补齐到固定宽度（最多 15 字节 = 45 字符）
        byteStr = byteStr.leftJustified(45, ' ');

        QString mnem  = QString::fromLatin1(insn[i].mnemonic);
        QString opStr = QString::fromLatin1(insn[i].op_str);

        html += QString("<span style=\"color:#6FA8DC;\">%1</span>  "
                        "<span style=\"color:#9AA0A6;\">%2</span>  "
                        "<span style=\"color:#E06C75;font-weight:bold;\">%3</span>  "
                        "<span style=\"color:#D7DAE0;\">%4</span><br>")
                    .arg(addr, byteStr, mnem, opStr);
    }

    html += "</pre>";

    m_text->setHtml(html);

    if (insn)
        cs_free(insn, count);

    cs_close(&handle);

    m_infoLabel->setText(QString("Address: %1    Bytes read: %2    Instructions: %3")
        .arg(hex(m_address)).arg(bytes.size()).arg(count));
}
