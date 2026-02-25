#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass


BOOL_TRUE = {"1", "true", "on", "yes"}
BOOL_FALSE = {"0", "false", "off", "no"}


@dataclass
class PllConfig:
    idx: int
    m: int | None
    n: int | None
    p: int | None
    q: int | None
    r: int | None
    fracn: int | None


def dedup_keep_order(values: list[str]) -> list[str]:
    out: list[str] = []
    seen: set[str] = set()
    for value in values:
        if value not in seen:
            seen.add(value)
            out.append(value)
    return out


def parse_int(token: str | None) -> int | None:
    if token is None:
        return None

    value = token.strip()
    if not value:
        return None

    cast_re = re.compile(r"^\(\s*[A-Za-z_][A-Za-z0-9_\s\*]*\s*\)\s*")
    while True:
        updated = cast_re.sub("", value)
        if updated == value:
            break
        value = updated.strip()

    if value.startswith("(") and value.endswith(")"):
        inner = value[1:-1].strip()
        if inner:
            value = inner

    value = re.sub(r"(?i)(ULL|LLU|UL|LU|LL|U|L)$", "", value).strip()

    if re.fullmatch(r"0[xX][0-9A-Fa-f]+", value):
        return int(value, 16)
    if re.fullmatch(r"[0-9]+", value):
        return int(value, 10)
    return None


def parse_bool(token: str | None) -> bool | None:
    if token is None:
        return None
    value = token.strip().lower()
    if value in BOOL_TRUE:
        return True
    if value in BOOL_FALSE:
        return False
    return None


def pick_first_number(values: list[str], default: int | None = None) -> int | None:
    for value in values:
        number = parse_int(value)
        if number is not None:
            return number
    return default


def pick_first_bool(values: list[str], default: bool | None = None) -> bool | None:
    for value in values:
        parsed = parse_bool(value)
        if parsed is not None:
            return parsed
    return default


def pick_first_text(values: list[str], default: str = "?") -> str:
    for value in values:
        stripped = value.strip()
        if stripped:
            return stripped
    return default


def parse_define_items(items: list[str]) -> tuple[set[str], dict[str, int], dict[str, str]]:
    present: set[str] = set()
    numeric: dict[str, int] = {}
    raw: dict[str, str] = {}

    for item in items:
        define = item.strip()
        if not define:
            continue

        if "=" in define:
            name, value = define.split("=", 1)
            name = name.strip()
            value = value.strip()
        else:
            name = define
            value = "1"

        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            continue

        present.add(name)
        raw[name] = value

        number = parse_int(value)
        if number is None:
            bool_value = parse_bool(value)
            if bool_value is not None:
                number = 1 if bool_value else 0
            else:
                number = 1

        numeric[name] = number

    return present, numeric, raw


def strip_outer_parens(expr: str) -> str:
    text = expr.strip()
    while text.startswith("(") and text.endswith(")"):
        depth = 0
        valid = True
        for index, char in enumerate(text):
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth < 0:
                    valid = False
                    break
            if depth == 0 and index != len(text) - 1:
                valid = False
                break
        if not valid or depth != 0:
            break
        text = text[1:-1].strip()
    return text


def split_top_level(expr: str, op: str) -> list[str]:
    parts: list[str] = []
    depth = 0
    start = 0
    i = 0
    while i < len(expr):
        char = expr[i]
        if char == "(":
            depth += 1
        elif char == ")":
            depth = max(depth - 1, 0)
        elif depth == 0 and expr.startswith(op, i):
            parts.append(expr[start:i])
            i += len(op)
            start = i
            continue
        i += 1
    parts.append(expr[start:])
    return parts


def eval_pp_atomic(expr: str, defined: set[str], numeric_defs: dict[str, int]) -> bool:
    text = strip_outer_parens(expr)

    negate = False
    while text.startswith("!"):
        negate = not negate
        text = strip_outer_parens(text[1:].strip())

    value = False

    match = re.fullmatch(r"defined\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", text)
    if match:
        value = match.group(1) in defined
    else:
        match = re.fullmatch(r"([A-Za-z_][A-Za-z0-9_]*)\s*([=!]=)\s*([A-Za-z_][A-Za-z0-9_]*|0[xX][0-9A-Fa-f]+|[0-9]+)", text)
        if match:
            lhs_name = match.group(1)
            op = match.group(2)
            rhs_token = match.group(3)

            lhs = numeric_defs.get(lhs_name, 0)
            rhs = parse_int(rhs_token)
            if rhs is None:
                rhs = numeric_defs.get(rhs_token, 0)

            if op == "==":
                value = lhs == rhs
            else:
                value = lhs != rhs
        else:
            number = parse_int(text)
            if number is not None:
                value = number != 0
            elif re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", text):
                value = numeric_defs.get(text, 0) != 0

    return (not value) if negate else value


def eval_pp_expr(expr: str, defined: set[str], numeric_defs: dict[str, int]) -> bool:
    text = strip_outer_parens(expr.strip())
    if not text:
        return False

    or_parts = split_top_level(text, "||")
    if len(or_parts) > 1:
        return any(eval_pp_expr(part, defined, numeric_defs) for part in or_parts)

    and_parts = split_top_level(text, "&&")
    if len(and_parts) > 1:
        return all(eval_pp_expr(part, defined, numeric_defs) for part in and_parts)

    return eval_pp_atomic(text, defined, numeric_defs)


def preprocess_text(text: str, defined: set[str], numeric_defs: dict[str, int]) -> str:
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    out: list[str] = []

    stack: list[dict[str, bool]] = []
    current_active = True

    for line in lines:
        stripped = line.lstrip()

        match_if = re.match(r"#\s*if\s+(.*)$", stripped)
        match_ifdef = re.match(r"#\s*ifdef\s+([A-Za-z_][A-Za-z0-9_]*)$", stripped)
        match_ifndef = re.match(r"#\s*ifndef\s+([A-Za-z_][A-Za-z0-9_]*)$", stripped)
        match_elif = re.match(r"#\s*elif\s+(.*)$", stripped)
        match_else = re.match(r"#\s*else\b", stripped)
        match_endif = re.match(r"#\s*endif\b", stripped)

        if match_if:
            parent_active = current_active
            cond = eval_pp_expr(match_if.group(1), defined, numeric_defs) if parent_active else False
            stack.append({"parent_active": parent_active, "taken": cond})
            current_active = parent_active and cond
            continue

        if match_ifdef:
            parent_active = current_active
            cond = match_ifdef.group(1) in defined if parent_active else False
            stack.append({"parent_active": parent_active, "taken": cond})
            current_active = parent_active and cond
            continue

        if match_ifndef:
            parent_active = current_active
            cond = (match_ifndef.group(1) not in defined) if parent_active else False
            stack.append({"parent_active": parent_active, "taken": cond})
            current_active = parent_active and cond
            continue

        if match_elif:
            if not stack:
                continue
            frame = stack[-1]
            if not frame["parent_active"] or frame["taken"]:
                current_active = False
            else:
                cond = eval_pp_expr(match_elif.group(1), defined, numeric_defs)
                current_active = frame["parent_active"] and cond
                if cond:
                    frame["taken"] = True
            continue

        if match_else:
            if not stack:
                continue
            frame = stack[-1]
            current_active = frame["parent_active"] and (not frame["taken"])
            frame["taken"] = True
            continue

        if match_endif:
            if stack:
                frame = stack.pop()
                current_active = frame["parent_active"]
            continue

        if current_active:
            out.append(line)

    return "\n".join(out)


def strip_comments(text: str) -> str:
    no_block = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    no_line = re.sub(r"//[^\n]*", "", no_block)
    return no_line


def collect_group1(pattern: str, text: str, flags: int = 0) -> list[str]:
    return dedup_keep_order([m.group(1).strip() for m in re.finditer(pattern, text, flags) if m.group(1).strip()])


def extract_designated(text: str, field: str) -> list[str]:
    return collect_group1(rf"\.{re.escape(field)}\s*=\s*([^,;}}]+)", text)


def token_to_freq_id(token: str) -> str | None:
    t = token.strip().upper()
    if not t:
        return None

    if t.startswith("UNI_HAL_RCC_CLKSRC_"):
        t = t[len("UNI_HAL_RCC_CLKSRC_"):]

    if any(x in t for x in ["PLL1P", "PLL1_P", "PLL1CLK", "PLLCLK"]):
        return "PLL1P"
    if any(x in t for x in ["PLL1Q", "PLL1_Q"]):
        return "PLL1Q"
    if any(x in t for x in ["PLL1R", "PLL1_R"]):
        return "PLL1R"
    if any(x in t for x in ["PLL2P", "PLL2_P", "PLL2CLK"]):
        return "PLL2P"
    if any(x in t for x in ["PLL2Q", "PLL2_Q"]):
        return "PLL2Q"
    if any(x in t for x in ["PLL2R", "PLL2_R"]):
        return "PLL2R"
    if any(x in t for x in ["PLL3P", "PLL3_P", "PLL3CLK"]):
        return "PLL3P"
    if any(x in t for x in ["PLL3Q", "PLL3_Q"]):
        return "PLL3Q"
    if any(x in t for x in ["PLL3R", "PLL3_R"]):
        return "PLL3R"
    if "SYSCLK" in t:
        return "SYSCLK"
    if "HCLK" in t:
        return "HCLK"
    if "PCLK1" in t:
        return "PCLK1"
    if "PCLK2" in t:
        return "PCLK2"
    if "PCLK3" in t:
        return "PCLK3"
    if "PCLK4" in t:
        return "PCLK4"
    if "USART16" in t:
        return "PCLK2"
    if "USART16910" in t:
        return "PCLK2"
    if "USART234578" in t:
        return "PCLK1"
    if "UART4" in t or "UART5" in t or "UART7" in t or "UART8" in t:
        return "PCLK1"
    if "USART1" in t or "USART6" in t:
        return "PCLK2"
    if "USART2" in t or "USART3" in t:
        return "PCLK1"
    if "SPI123" in t:
        return "PLL1Q"
    if "SPI45" in t:
        return "PCLK2"
    if "SPI6" in t:
        return "PCLK4"
    if t == "PCLK":
        return "PCLK1"
    if t == "SPI" or t == "USART" or t == "UART":
        return None
    if "HSE" in t:
        return "HSE"
    if "HSI48" in t:
        return "HSI48"
    if "HSI" in t:
        return "HSI"
    if "CSI" in t:
        return "CSI"
    if "LSE" in t:
        return "LSE"
    if "LSI" in t:
        return "LSI"
    return None


def freq_id_to_node(freq_id: str) -> str | None:
    mapping = {
        "HSE": "src_hse",
        "HSI": "src_hsi",
        "HSI48": "src_hsi48",
        "CSI": "src_csi",
        "LSE": "src_lse",
        "LSI": "src_lsi",
        "PLL1P": "pll1_p",
        "PLL1Q": "pll1_q",
        "PLL1R": "pll1_r",
        "PLL2P": "pll2_p",
        "PLL2Q": "pll2_q",
        "PLL2R": "pll2_r",
        "PLL3P": "pll3_p",
        "PLL3Q": "pll3_q",
        "PLL3R": "pll3_r",
        "SYSCLK": "bus_sysclk",
        "HCLK": "bus_hclk",
        "PCLK1": "bus_pclk1",
        "PCLK2": "bus_pclk2",
        "PCLK3": "bus_pclk3",
        "PCLK4": "bus_pclk4",
    }
    return mapping.get(freq_id)


def bus_to_node(bus_name: str) -> tuple[str, str]:
    bus = bus_name.upper()
    if re.match(r"^(D[123])?AHB", bus):
        return "bus_hclk", "HCLK"
    if re.match(r"^(D[123])?APB1", bus):
        return "bus_pclk1", "PCLK1"
    if re.match(r"^(D[123])?APB2", bus):
        return "bus_pclk2", "PCLK2"
    if re.match(r"^(D[123])?APB3", bus):
        return "bus_pclk3", "PCLK3"
    if re.match(r"^(D[123])?APB4", bus):
        return "bus_pclk4", "PCLK4"
    return "bus_hclk", "HCLK"


def parse_mco_div(token: str) -> int:
    number = parse_int(token)
    if number is not None and number > 0:
        return number

    upper = token.strip().upper()
    for pattern in [r"DIV([0-9]+)", r"MCODIV_([0-9]+)", r"_DIV_([0-9]+)"]:
        match = re.search(pattern, upper)
        if match:
            return int(match.group(1), 10)
    return 1


def format_hz(value: int | None) -> str:
    if value is None:
        return "?"
    hz = int(value)
    if hz >= 1_000_000_000:
        return f"{hz / 1_000_000_000:g} GHz"
    if hz >= 1_000_000:
        return f"{hz / 1_000_000:g} MHz"
    if hz >= 1_000:
        return f"{hz / 1_000:g} kHz"
    return f"{hz} Hz"


def esc_label(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def div_hz(value: int | None, divider: int | None) -> int | None:
    if value is None or divider is None or divider <= 0:
        return None
    return value // divider


def mul_div_hz(value: int | None, mul: int | None, div: int | None) -> int | None:
    if value is None or mul is None or div is None or div <= 0:
        return None
    return (value * mul) // div


def parse_pll_configs(text: str) -> list[PllConfig]:
    records: list[PllConfig] = []
    for match in re.finditer(r"\.pll\s*\[\s*([0-9]+)\s*\]\s*=\s*\{(.*?)\}", text, flags=re.S):
        idx = int(match.group(1), 10) + 1
        body = match.group(2)

        m = pick_first_number(extract_designated(body, "m"))
        n = pick_first_number(extract_designated(body, "n"))
        p = pick_first_number(extract_designated(body, "p"))
        q = pick_first_number(extract_designated(body, "q"))
        r = pick_first_number(extract_designated(body, "r"))
        fracn = pick_first_number(extract_designated(body, "fracn"))

        if any(v is not None for v in [m, n, p, q, r, fracn]):
            records.append(PllConfig(idx=idx, m=m, n=n, p=p, q=q, r=r, fracn=fracn))

    records.sort(key=lambda r: r.idx)
    return records


def parse_peripherals(text: str) -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []

    for periph in collect_group1(r"__HAL_RCC_([A-Za-z0-9_]+)_CLK_ENABLE\s*\(", text):
        pairs.append(("UNKNOWN", periph.upper()))

    for match in re.finditer(r"LL_([A-Z0-9]+)_GRP[0-9]+_EnableClock\s*\(([^\)]*)\)", text, flags=re.S):
        bus = match.group(1)
        args = match.group(2)
        for periph in collect_group1(r"LL_[A-Z0-9]+_GRP[0-9]+_PERIPH_([A-Z0-9_]+)", args):
            pairs.append((bus, periph))

    for match in re.finditer(r"RCC_([A-Z0-9]+ENR[0-9]*)_([A-Z0-9_]+)EN", text):
        reg_name = match.group(1)
        periph = match.group(2)
        bus = re.sub(r"ENR[0-9]*$", "", reg_name)
        pairs.append((bus, periph))

    out: list[tuple[str, str]] = []
    seen: set[tuple[str, str]] = set()
    for pair in pairs:
        if pair not in seen:
            seen.add(pair)
            out.append(pair)
    return out


@dataclass
class UartClockConsumer:
    name: str
    instance: str | None
    clksrc_token: str
    baudrate: int | None
    baudrate_div: int | None


@dataclass
class SpiClockConsumer:
    name: str
    instance: str | None
    clksrc_token: str
    prescaler: int | None


@dataclass
class I2cClockConsumer:
    name: str
    instance: str | None
    clksrc_token: str | None
    speed_hz: int | None


@dataclass
class AdcClockConsumer:
    name: str
    instance: str | None
    clksrc_token: str | None
    channels_count: int | None


def _extract_block(text: str, start_index: int) -> tuple[str, int] | None:
    eq_index = text.find("=", start_index)
    if eq_index < 0:
        return None

    brace_open = text.find("{", eq_index)
    if brace_open < 0:
        return None

    depth = 0
    i = brace_open
    while i < len(text):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[brace_open + 1 : i], i + 1
        i += 1

    return None


def _extract_designated_from_block(block_text: str, field: str) -> str | None:
    values = extract_designated(block_text, field)
    if not values:
        return None
    return values[0].strip()


def parse_uart_consumers(text: str) -> list[UartClockConsumer]:
    consumers: list[UartClockConsumer] = []
    pattern = re.compile(r"uni_hal_usart_context_t\s+([A-Za-z_][A-Za-z0-9_]*)\s*=", flags=re.S)

    for match in pattern.finditer(text):
        name = match.group(1).strip()
        block = _extract_block(text, match.end() - 1)
        if block is None:
            continue

        block_text, _ = block
        clksrc_token = _extract_designated_from_block(block_text, "clksrc")
        if clksrc_token is None:
            continue

        instance = _extract_designated_from_block(block_text, "instance")
        baudrate = parse_int(_extract_designated_from_block(block_text, "baudrate"))
        baudrate_div = parse_int(_extract_designated_from_block(block_text, "baudrate_div"))

        consumers.append(
            UartClockConsumer(
                name=name,
                instance=instance,
                clksrc_token=clksrc_token,
                baudrate=baudrate,
                baudrate_div=baudrate_div,
            )
        )

    return consumers


def parse_spi_consumers(text: str) -> list[SpiClockConsumer]:
    consumers: list[SpiClockConsumer] = []
    pattern = re.compile(r"uni_hal_spi_context_t\s+([A-Za-z_][A-Za-z0-9_]*)\s*=", flags=re.S)

    for match in pattern.finditer(text):
        name = match.group(1).strip()
        block = _extract_block(text, match.end() - 1)
        if block is None:
            continue

        block_text, _ = block
        clksrc_token = _extract_designated_from_block(block_text, "clock_source")
        if clksrc_token is None:
            continue

        instance = _extract_designated_from_block(block_text, "instance")
        prescaler = parse_int(_extract_designated_from_block(block_text, "prescaler"))

        if prescaler is None:
            prescaler_token = _extract_designated_from_block(block_text, "prescaler")
            if prescaler_token is not None:
                prescaler_match = re.search(r"_([0-9]+)$", prescaler_token.strip().upper())
                if prescaler_match:
                    prescaler = int(prescaler_match.group(1), 10)

        consumers.append(
            SpiClockConsumer(
                name=name,
                instance=instance,
                clksrc_token=clksrc_token,
                prescaler=prescaler,
            )
        )

    return consumers


def parse_i2c_speed_hz(token: str | None) -> int | None:
    number = parse_int(token)
    if number is not None and number > 0:
        return number

    if token is None:
        return None

    upper = token.strip().upper()
    match = re.search(r"_([0-9]+)\s*([KM])HZ$", upper)
    if not match:
        return None

    value = int(match.group(1), 10)
    unit = match.group(2)
    if unit == "K":
        return value * 1_000
    return value * 1_000_000


def parse_i2c_consumers(text: str) -> list[I2cClockConsumer]:
    consumers: list[I2cClockConsumer] = []
    pattern = re.compile(r"uni_hal_i2c_context_t\s+([A-Za-z_][A-Za-z0-9_]*)\s*=", flags=re.S)

    for match in pattern.finditer(text):
        name = match.group(1).strip()
        block = _extract_block(text, match.end() - 1)
        if block is None:
            continue

        block_text, _ = block
        instance = _extract_designated_from_block(block_text, "instance")
        clksrc_token = _extract_designated_from_block(block_text, "clock_source")
        speed_hz = parse_i2c_speed_hz(_extract_designated_from_block(block_text, "speed"))

        if instance is None and clksrc_token is None:
            continue

        consumers.append(
            I2cClockConsumer(
                name=name,
                instance=instance,
                clksrc_token=clksrc_token,
                speed_hz=speed_hz,
            )
        )

    return consumers


def parse_adc_consumers(text: str) -> list[AdcClockConsumer]:
    consumers: list[AdcClockConsumer] = []
    pattern = re.compile(r"uni_hal_adc_context_t\s+([A-Za-z_][A-Za-z0-9_]*)\s*=", flags=re.S)

    for match in pattern.finditer(text):
        name = match.group(1).strip()
        block = _extract_block(text, match.end() - 1)
        if block is None:
            continue

        block_text, _ = block
        instance = _extract_designated_from_block(block_text, "instance")
        clksrc_token = _extract_designated_from_block(block_text, "clock_source")
        channels_count = parse_int(_extract_designated_from_block(block_text, "channels_count"))

        if instance is None and clksrc_token is None:
            continue

        consumers.append(
            AdcClockConsumer(
                name=name,
                instance=instance,
                clksrc_token=clksrc_token,
                channels_count=channels_count,
            )
        )

    return consumers


def normalize_instance_name(instance: str | None, fallback: str) -> str:
    if instance is None:
        return fallback

    token = instance.strip().upper()
    if token.startswith("UNI_HAL_CORE_PERIPH_"):
        token = token[len("UNI_HAL_CORE_PERIPH_") :]

    token = token.replace("_", "")
    return token if token else fallback


def uart_instance_to_freq_id(instance: str | None) -> str | None:
    if instance is None:
        return None

    token = normalize_instance_name(instance, "")
    if token in {"USART1", "USART6"}:
        return "PCLK2"
    if token in {"USART2", "USART3", "UART4", "UART5", "UART7", "UART8"}:
        return "PCLK1"
    return None


def spi_instance_to_freq_id(instance: str | None) -> str | None:
    if instance is None:
        return None

    token = normalize_instance_name(instance, "")
    if token in {"SPI1", "SPI2", "SPI3"}:
        return "PLL1Q"
    if token in {"SPI4", "SPI5"}:
        return "PCLK2"
    if token in {"SPI6"}:
        return "PCLK4"
    return None


def i2c_instance_to_freq_id(instance: str | None) -> str | None:
    if instance is None:
        return None

    token = normalize_instance_name(instance, "")
    if token in {"I2C1", "I2C2", "I2C3"}:
        return "PCLK1"
    if token in {"I2C4"}:
        return "PCLK4"
    return None


def write_text(path: pathlib.Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def find_project_file_near_inputs(input_paths: list[pathlib.Path], relative_path: str) -> pathlib.Path | None:
    seen: set[pathlib.Path] = set()
    for input_path in input_paths:
        for parent in input_path.parents:
            candidate = (parent / relative_path).resolve()
            if candidate in seen:
                continue
            seen.add(candidate)
            if candidate.exists():
                return candidate
    return None


def detect_divider(text: str, patterns: list[str]) -> int | None:
    for pattern in patterns:
        value = pick_first_number(collect_group1(pattern, text))
        if value is not None:
            return value
    return None


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate RCC map DOT/PNG/SVG")
    parser.add_argument("--target-name", required=True)
    parser.add_argument("--primary-input", default="")
    parser.add_argument("--input-file", action="append", default=[])

    parser.add_argument("--dot-file", required=True)
    parser.add_argument("--png-file", required=True)
    parser.add_argument("--svg-file", required=True)
    parser.add_argument("--dot-exe", default="")

    parser.add_argument("--define", action="append", default=[])

    parser.add_argument("--hse-hz", default="")
    parser.add_argument("--hsi-hz", default="")
    parser.add_argument("--hsi48-hz", default="")
    parser.add_argument("--csi-hz", default="")
    parser.add_argument("--lsi-hz", default="")
    parser.add_argument("--lse-hz", default="")
    return parser


def main() -> int:
    args = build_argument_parser().parse_args()

    input_files = list(args.input_file)
    if not input_files and args.primary_input:
        input_files = [args.primary_input]
    if not input_files:
        print("RCC map generator: no input files provided", file=sys.stderr)
        return 2

    input_paths = [pathlib.Path(path).resolve() for path in input_files]
    for path in input_paths:
        if not path.exists():
            print(f"RCC map generator: input file does not exist: {path}", file=sys.stderr)
            return 2

    defined, numeric_defs, _raw_defs = parse_define_items(args.define)

    preprocessed_parts: list[str] = []
    raw_parts: list[str] = []
    for path in input_paths:
        raw_text = path.read_text(encoding="utf-8")
        raw_parts.append(raw_text)
        preprocessed_parts.append(preprocess_text(raw_text, defined, numeric_defs))

    merged_raw = "\n".join(raw_parts)
    merged_pp = "\n".join(preprocessed_parts)

    merged_pp = strip_comments(merged_pp)
    merged_raw = strip_comments(merged_raw)

    extra_clock_text_parts: list[str] = []
    if "STM32H7" in defined:
        h7_rcc_path = find_project_file_near_inputs(input_paths, "src_unihal/src/rcc/uni_hal_rcc_stm32h7.c")
        if h7_rcc_path is not None:
            extra_clock_text_parts.append(strip_comments(h7_rcc_path.read_text(encoding="utf-8")))
    if "STM32L4" in defined:
        l4_rcc_path = find_project_file_near_inputs(input_paths, "src_unihal/src/rcc/uni_hal_rcc_stm32l4.c")
        if l4_rcc_path is not None:
            extra_clock_text_parts.append(strip_comments(l4_rcc_path.read_text(encoding="utf-8")))

    divider_scan_text = "\n".join([merged_pp, merged_raw, *extra_clock_text_parts])

    hse_enable_values = extract_designated(merged_pp, "hse_enable") or extract_designated(merged_raw, "hse_enable")
    hse_bypass_values = extract_designated(merged_pp, "hse_bypass") or extract_designated(merged_raw, "hse_bypass")
    lse_enable_values = extract_designated(merged_pp, "lse_enable") or extract_designated(merged_raw, "lse_enable")
    csi_enable_values = extract_designated(merged_pp, "csi_enable") or extract_designated(merged_raw, "csi_enable")

    hse_enable = pick_first_bool(hse_enable_values)
    hse_bypass = pick_first_bool(hse_bypass_values)
    lse_enable = pick_first_bool(lse_enable_values)
    csi_enable = pick_first_bool(csi_enable_values)

    pll_source_values = collect_group1(r"RCC_PLLSOURCE_([A-Z0-9_]+)", merged_pp)
    if not pll_source_values:
        pll_source_values = collect_group1(r"LL_RCC_PLLSOURCE_([A-Z0-9_]+)", merged_pp)
    if not pll_source_values:
        pll_source_values = ["HSE" if (hse_enable is True) else "HSI"]

    sys_source_values = collect_group1(r"RCC_SYSCLKSOURCE_([A-Z0-9_]+)", merged_pp)
    if not sys_source_values:
        sys_source_values = collect_group1(r"RCC_CFGR_SW_([A-Z0-9_]+)", merged_pp)
    if not sys_source_values:
        sys_source_values = collect_group1(r"LL_RCC_SYS_CLKSOURCE_([A-Z0-9_]+)", merged_pp)

    stm32h7_target = "STM32H7" in defined
    default_sys_div = 1
    default_hclk_div = 2 if stm32h7_target else 1
    default_apb_div = 2 if stm32h7_target else 1

    sys_div = detect_divider(
        divider_scan_text,
        [
            r"RCC_SYSCLK_DIV([0-9]+)",
            r"D1CPRE_DIV([0-9]+)",
            r"LL_RCC_SetSysPrescaler\s*\(\s*LL_RCC_SYSCLK_DIV_([0-9]+)\s*\)",
        ],
    )
    if sys_div is None:
        sys_div = default_sys_div

    hclk_div = detect_divider(
        divider_scan_text,
        [
            r"RCC_HCLK_DIV([0-9]+)",
            r"HPRE_DIV([0-9]+)",
            r"LL_RCC_SetAHBPrescaler\s*\(\s*LL_RCC_AHB_DIV_([0-9]+)\s*\)",
            r"LL_RCC_SetAHBPrescaler\s*\(\s*LL_RCC_SYSCLK_DIV_([0-9]+)\s*\)",
        ],
    )
    if hclk_div is None:
        hclk_div = default_hclk_div

    apb1_div = detect_divider(
        divider_scan_text,
        [
            r"RCC_APB1_DIV([0-9]+)",
            r"LL_RCC_SetAPB1Prescaler\s*\(\s*LL_RCC_APB1_DIV_([0-9]+)\s*\)",
        ],
    )
    if apb1_div is None:
        apb1_div = default_apb_div

    apb2_div = detect_divider(
        divider_scan_text,
        [
            r"RCC_APB2_DIV([0-9]+)",
            r"LL_RCC_SetAPB2Prescaler\s*\(\s*LL_RCC_APB2_DIV_([0-9]+)\s*\)",
        ],
    )
    if apb2_div is None:
        apb2_div = default_apb_div

    apb3_div = detect_divider(
        divider_scan_text,
        [
            r"RCC_APB3_DIV([0-9]+)",
            r"LL_RCC_SetAPB3Prescaler\s*\(\s*LL_RCC_APB3_DIV_([0-9]+)\s*\)",
        ],
    )
    if apb3_div is None:
        apb3_div = default_apb_div

    apb4_div = detect_divider(
        divider_scan_text,
        [
            r"RCC_APB4_DIV([0-9]+)",
            r"LL_RCC_SetAPB4Prescaler\s*\(\s*LL_RCC_APB4_DIV_([0-9]+)\s*\)",
        ],
    )
    if apb4_div is None:
        apb4_div = default_apb_div

    pll_records = parse_pll_configs(merged_pp)
    if not pll_records:
        pll_records = parse_pll_configs(merged_raw)

    mco_sources: dict[int, str | None] = {1: None, 2: None}
    mco_divs: dict[int, int] = {1: 1, 2: 1}

    for match in re.finditer(r"uni_hal_rcc_stm32_mco_enable\s*\(\s*([12])\s*,\s*([^,\)]+)\s*,\s*([^,\)]+)\s*\)", merged_raw):
        idx = int(match.group(1), 10)
        src = match.group(2).strip()
        div = match.group(3).strip()
        mco_sources[idx] = src
        mco_divs[idx] = parse_mco_div(div)

    for match in re.finditer(r"__HAL_RCC_MCO([12])_CONFIG\s*\(([^\)]*)\)", merged_raw, flags=re.S):
        idx = int(match.group(1), 10)
        args_text = match.group(2)
        parts = [p.strip() for p in args_text.split(",")]
        if len(parts) >= 1 and parts[0]:
            mco_sources[idx] = parts[0]
        if len(parts) >= 2 and parts[1]:
            mco_divs[idx] = parse_mco_div(parts[1])

    for match in re.finditer(r"LL_RCC_ConfigMCO\s*\(([^\)]*)\)", merged_raw, flags=re.S):
        parts = [p.strip() for p in match.group(1).split(",")]
        if not parts:
            continue
        src = parts[0].upper()
        idx = 1 if "MCO1" in src else (2 if "MCO2" in src else 0)
        if idx == 0:
            continue
        mco_sources[idx] = parts[0]
        if len(parts) >= 2:
            mco_divs[idx] = parse_mco_div(parts[1])

    uart_consumers = parse_uart_consumers(merged_pp)
    if not uart_consumers:
        uart_consumers = parse_uart_consumers(merged_raw)

    spi_consumers = parse_spi_consumers(merged_pp)
    if not spi_consumers:
        spi_consumers = parse_spi_consumers(merged_raw)

    i2c_consumers = parse_i2c_consumers(merged_pp)
    if not i2c_consumers:
        i2c_consumers = parse_i2c_consumers(merged_raw)

    adc_consumers = parse_adc_consumers(merged_pp)
    if not adc_consumers:
        adc_consumers = parse_adc_consumers(merged_raw)

    hse_hz = parse_int(args.hse_hz)
    if hse_hz is None:
        hse_hz = numeric_defs.get("HSE_VALUE")
    if hse_hz is None:
        hse_hz = 25_000_000

    hsi_hz = parse_int(args.hsi_hz) or numeric_defs.get("HSI_VALUE") or 64_000_000
    hsi48_hz = parse_int(args.hsi48_hz) or numeric_defs.get("HSI48_VALUE") or 48_000_000
    csi_hz = parse_int(args.csi_hz) or numeric_defs.get("CSI_VALUE") or 4_000_000
    lsi_hz = parse_int(args.lsi_hz) or numeric_defs.get("LSI_VALUE") or 32_000
    lse_hz = parse_int(args.lse_hz) or numeric_defs.get("LSE_VALUE") or 32_768

    freqs: dict[str, int | None] = {
        "HSI": hsi_hz,
        "HSE": hse_hz if hse_enable is not False else None,
        "HSI48": hsi48_hz,
        "CSI": csi_hz if csi_enable is not False else None,
        "LSI": lsi_hz,
        "LSE": lse_hz if lse_enable is not False else None,
    }

    pll_source_hz: int | None = None
    pll_source_node = "src_hsi"
    for token in pll_source_values:
        freq_id = token_to_freq_id(token)
        if freq_id is None:
            continue
        candidate = freqs.get(freq_id)
        if candidate is not None:
            pll_source_hz = candidate
            node = freq_id_to_node(freq_id)
            if node:
                pll_source_node = node
            break

    if pll_source_hz is None:
        if freqs.get("HSE") is not None:
            pll_source_hz = freqs["HSE"]
            pll_source_node = "src_hse"
        else:
            pll_source_hz = freqs["HSI"]
            pll_source_node = "src_hsi"

    pll_outputs: dict[str, int | None] = {}
    pll1_p_div: int | None = None
    for pll in pll_records:
        vco = mul_div_hz(pll_source_hz, pll.n, pll.m)
        p_hz = div_hz(vco, pll.p)
        q_hz = div_hz(vco, pll.q)
        r_hz = div_hz(vco, pll.r)

        if pll.idx == 1:
            pll1_p_div = pll.p

        if pll.p and pll.p > 0:
            pll_outputs[f"PLL{pll.idx}P"] = p_hz
        if pll.q and pll.q > 0:
            pll_outputs[f"PLL{pll.idx}Q"] = q_hz
        if pll.r and pll.r > 0:
            pll_outputs[f"PLL{pll.idx}R"] = r_hz

    freqs.update(pll_outputs)

    sys_source_node = "src_hsi"
    sys_source_hz: int | None = None
    for token in sys_source_values:
        freq_id = token_to_freq_id(token)
        if freq_id is None:
            continue
        candidate = freqs.get(freq_id)
        if candidate is not None:
            sys_source_hz = candidate
            node = freq_id_to_node(freq_id)
            if node:
                sys_source_node = node
            break

    if sys_source_hz is None:
        if freqs.get("PLL1P") is not None:
            sys_source_hz = freqs["PLL1P"]
            sys_source_node = "pll1_p"
        else:
            sys_source_hz = freqs.get("HSI")
            sys_source_node = "src_hsi"

    sysclk_hz = sys_source_hz
    hclk_hz = div_hz(div_hz(sysclk_hz, sys_div), hclk_div)
    pclk1_hz = div_hz(hclk_hz, apb1_div)
    pclk2_hz = div_hz(hclk_hz, apb2_div)
    pclk3_hz = div_hz(hclk_hz, apb3_div)
    pclk4_hz = div_hz(hclk_hz, apb4_div)

    freqs.update(
        {
            "SYSCLK": sysclk_hz,
            "HCLK": hclk_hz,
            "PCLK1": pclk1_hz,
            "PCLK2": pclk2_hz,
            "PCLK3": pclk3_hz,
            "PCLK4": pclk4_hz,
        }
    )

    hsi_label = "HSI"
    hse_label = "HSE"
    lsi_label = "LSI"
    lse_label = "LSE"
    csi_label = "CSI"
    hsi48_label = "HSI48"

    if freqs.get("HSI") is not None:
        hsi_label += f"\n{format_hz(freqs['HSI'])}"
    if freqs.get("HSE") is not None:
        hse_label += f"\n{format_hz(freqs['HSE'])}"
    if freqs.get("LSI") is not None:
        lsi_label += f"\n{format_hz(freqs['LSI'])}"
    if freqs.get("LSE") is not None:
        lse_label += f"\n{format_hz(freqs['LSE'])}"
    if freqs.get("CSI") is not None:
        csi_label += f"\n{format_hz(freqs['CSI'])}"
    if freqs.get("HSI48") is not None:
        hsi48_label += f"\n{format_hz(freqs['HSI48'])}"

    graph_title = "RCC clock configuration"

    lines: list[str] = []
    lines.append("digraph RCCMap {")
    lines.append(
        f'  graph [rankdir=LR, splines=true, pad=0.2, nodesep=0.45, ranksep=0.7, fontname="Helvetica", fontsize=10, labelloc=t, label="{esc_label(graph_title)}"];'
    )
    lines.append('  node [fontname="Helvetica", fontsize=10, style=filled, color="#475569"];')
    lines.append('  edge [fontname="Helvetica", fontsize=9, color="#4b5563"];')
    lines.append("")

    source_nodes: list[tuple[str, str]] = []
    if freqs.get("HSI") is not None:
        source_nodes.append(("src_hsi", hsi_label))
    if freqs.get("HSE") is not None:
        source_nodes.append(("src_hse", hse_label))
    if freqs.get("HSI48") is not None:
        source_nodes.append(("src_hsi48", hsi48_label))
    if freqs.get("LSI") is not None:
        source_nodes.append(("src_lsi", lsi_label))
    if freqs.get("LSE") is not None:
        source_nodes.append(("src_lse", lse_label))
    if freqs.get("CSI") is not None:
        source_nodes.append(("src_csi", csi_label))

    if source_nodes:
        lines.append("  subgraph cluster_sources {")
        lines.append('    label="Clock sources";')
        lines.append('    color="#cbd5e1";')
        lines.append('    style="rounded";')
        for node_name, node_label in source_nodes:
            lines.append(f'    "{node_name}" [label="{esc_label(node_label)}", shape=box, fillcolor="#dbeafe"];')
        lines.append("  }")
        lines.append("")

    lines.append("  subgraph cluster_pll {")
    lines.append('    label="PLL";')
    lines.append('    color="#cbd5e1";')
    lines.append('    style="rounded";')

    if pll_records:
        for pll in pll_records:
            vco_hz = mul_div_hz(pll_source_hz, pll.n, pll.m)
            p_hz = div_hz(vco_hz, pll.p)
            q_hz = div_hz(vco_hz, pll.q)
            r_hz = div_hz(vco_hz, pll.r)

            pll_label = f"PLL{pll.idx}\nN={pll.n if pll.n is not None else '?'}"
            if vco_hz is not None:
                pll_label += f"\nVCO={format_hz(vco_hz)}"
            if pll.fracn not in (None, 0):
                pll_label += f"\nFRACN={pll.fracn}"
            lines.append(f'    "pll{pll.idx}" [label="{esc_label(pll_label)}", shape=component, fillcolor="#dcfce7"];')

            if pll.p not in (None, 0):
                node = f"pll{pll.idx}_p"
                label = f"PLL{pll.idx}P\n{format_hz(p_hz)}"
                lines.append(f'    "{node}" [label="{esc_label(label)}", shape=oval, fillcolor="#bbf7d0", color="#16a34a"];')
                lines.append(f'  "pll{pll.idx}" -> "{node}" [label="/{pll.p}", color="#16a34a"];')
            if pll.q not in (None, 0):
                node = f"pll{pll.idx}_q"
                label = f"PLL{pll.idx}Q\n{format_hz(q_hz)}"
                lines.append(f'    "{node}" [label="{esc_label(label)}", shape=oval, fillcolor="#bbf7d0", color="#16a34a"];')
                lines.append(f'  "pll{pll.idx}" -> "{node}" [label="/{pll.q}", color="#16a34a"];')
            if pll.r not in (None, 0):
                node = f"pll{pll.idx}_r"
                label = f"PLL{pll.idx}R\n{format_hz(r_hz)}"
                lines.append(f'    "{node}" [label="{esc_label(label)}", shape=oval, fillcolor="#bbf7d0", color="#16a34a"];')
                lines.append(f'  "pll{pll.idx}" -> "{node}" [label="/{pll.r}", color="#16a34a"];')

            src_div = pll.m if (pll.m is not None and pll.m > 0) else 1
            src_label = f"/{src_div}"
            lines.append(f'  "{pll_source_node}" -> "pll{pll.idx}" [label="{esc_label(src_label)}"];')
    else:
        lines.append('    "pll_none" [label="PLL config not detected", shape=note, fillcolor="#f8fafc", color="#94a3b8"];')

    lines.append("  }")
    lines.append("")

    lines.append("  subgraph cluster_buses {")
    lines.append('    label="System / bus clocks";')
    lines.append('    color="#cbd5e1";')
    lines.append('    style="rounded";')
    lines.append('    "bus_sysclk" [label="SYSCLK", shape=box3d, fillcolor="#e0f2fe"];')
    lines.append('    "bus_hclk" [label="HCLK", shape=box3d, fillcolor="#e0f2fe"];')
    lines.append('    "bus_pclk1" [label="PCLK1", shape=box3d, fillcolor="#e0f2fe"];')
    lines.append('    "bus_pclk2" [label="PCLK2", shape=box3d, fillcolor="#e0f2fe"];')
    lines.append('    "bus_pclk3" [label="PCLK3", shape=box3d, fillcolor="#e0f2fe"];')
    lines.append('    "bus_pclk4" [label="PCLK4", shape=box3d, fillcolor="#e0f2fe"];')
    lines.append("  }")
    lines.append("")

    configured_mcos = [idx for idx in [1, 2] if mco_sources[idx]]
    if configured_mcos:
        lines.append("  subgraph cluster_mco {")
        lines.append('    label="MCO outputs";')
        lines.append('    color="#fecaca";')
        lines.append('    style="rounded";')

        for idx in configured_mcos:
            src_token = mco_sources[idx]
            if src_token is None:
                continue

            freq_id = token_to_freq_id(src_token)
            src_node = freq_id_to_node(freq_id) if freq_id else None
            src_hz = freqs.get(freq_id) if freq_id else None
            mco_hz = div_hz(src_hz, mco_divs[idx])
            label = f"MCO{idx}"
            if mco_hz is not None:
                label += f"\n{format_hz(mco_hz)}"
            lines.append(f'    "mco{idx}" [label="{esc_label(label)}", shape=octagon, fillcolor="#fee2e2", color="#b91c1c"];')

            if src_node:
                lines.append(f'  "{src_node}" -> "mco{idx}" [label="/{mco_divs[idx]}", color="#b91c1c", penwidth=1.2];')

        lines.append("  }")
        lines.append("")

    lines.append(f'  "{sys_source_node}" -> "bus_sysclk" [label="/1", penwidth=1.2];')

    sysclk_node_label = f"SYSCLK\n{format_hz(sysclk_hz)}"
    hclk_node_label = f"HCLK\n{format_hz(hclk_hz)}"
    pclk1_node_label = f"PCLK1\n{format_hz(pclk1_hz)}"
    pclk2_node_label = f"PCLK2\n{format_hz(pclk2_hz)}"
    pclk3_node_label = f"PCLK3\n{format_hz(pclk3_hz)}"
    pclk4_node_label = f"PCLK4\n{format_hz(pclk4_hz)}"

    combined_hclk_div = sys_div * hclk_div
    sys_to_hclk_label = f"/{combined_hclk_div}"
    hclk_to_pclk1_label = f"/{apb1_div}"
    hclk_to_pclk2_label = f"/{apb2_div}"
    hclk_to_pclk3_label = f"/{apb3_div}"
    hclk_to_pclk4_label = f"/{apb4_div}"

    lines.append(f'  "bus_sysclk" [label="{esc_label(sysclk_node_label)}"];')
    lines.append(f'  "bus_hclk" [label="{esc_label(hclk_node_label)}"];')
    lines.append(f'  "bus_pclk1" [label="{esc_label(pclk1_node_label)}"];')
    lines.append(f'  "bus_pclk2" [label="{esc_label(pclk2_node_label)}"];')
    lines.append(f'  "bus_pclk3" [label="{esc_label(pclk3_node_label)}"];')
    lines.append(f'  "bus_pclk4" [label="{esc_label(pclk4_node_label)}"];')

    lines.append(f'  "bus_sysclk" -> "bus_hclk" [label="{esc_label(sys_to_hclk_label)}", penwidth=1.2];')
    lines.append(f'  "bus_hclk" -> "bus_pclk1" [label="{esc_label(hclk_to_pclk1_label)}"];')
    lines.append(f'  "bus_hclk" -> "bus_pclk2" [label="{esc_label(hclk_to_pclk2_label)}"];')
    lines.append(f'  "bus_hclk" -> "bus_pclk3" [label="{esc_label(hclk_to_pclk3_label)}"];')
    lines.append(f'  "bus_hclk" -> "bus_pclk4" [label="{esc_label(hclk_to_pclk4_label)}"];')
    lines.append("")

    if uart_consumers:
        lines.append("  subgraph cluster_uart_io {")
        lines.append('    label="UART clocks";')
        lines.append('    color="#cbd5e1";')
        lines.append('    style="rounded";')

        for idx, uart in enumerate(uart_consumers, start=1):
            node_name = f"uart_cfg_{idx}"
            inst_name = normalize_instance_name(uart.instance, f"UART{idx}")

            freq_id = token_to_freq_id(uart.clksrc_token)
            if freq_id is None:
                freq_id = uart_instance_to_freq_id(uart.instance)

            src_node = freq_id_to_node(freq_id) if freq_id else None
            src_hz = freqs.get(freq_id) if freq_id else None

            uart_div = uart.baudrate_div if (uart.baudrate_div is not None and uart.baudrate_div > 0) else 1
            uart_clk_hz = div_hz(src_hz, uart_div)

            label = inst_name
            if uart_clk_hz is not None:
                label += f"\n{format_hz(uart_clk_hz)}"
            elif src_hz is not None:
                label += f"\n{format_hz(src_hz)}"

            if uart.baudrate is not None:
                label += f"\n{uart.baudrate} bps"

            lines.append(f'    "{node_name}" [label="{esc_label(label)}", shape=ellipse, fillcolor="#e0f2fe", color="#0369a1"];')
            if src_node:
                lines.append(f'  "{src_node}" -> "{node_name}" [label="/{uart_div}", color="#0369a1"];')

        lines.append("  }")
        lines.append("")

    if spi_consumers:
        lines.append("  subgraph cluster_spi_io {")
        lines.append('    label="SPI clocks";')
        lines.append('    color="#cbd5e1";')
        lines.append('    style="rounded";')

        for idx, spi in enumerate(spi_consumers, start=1):
            node_name = f"spi_cfg_{idx}"
            inst_name = normalize_instance_name(spi.instance, f"SPI{idx}")

            freq_id = token_to_freq_id(spi.clksrc_token)
            if freq_id is None:
                freq_id = spi_instance_to_freq_id(spi.instance)

            src_node = freq_id_to_node(freq_id) if freq_id else None
            src_hz = freqs.get(freq_id) if freq_id else None

            spi_div = spi.prescaler if (spi.prescaler is not None and spi.prescaler > 0) else 1
            spi_clk_hz = div_hz(src_hz, spi_div)

            label = inst_name
            if spi_clk_hz is not None:
                label += f"\n{format_hz(spi_clk_hz)}"
            elif src_hz is not None:
                label += f"\n{format_hz(src_hz)}"

            lines.append(f'    "{node_name}" [label="{esc_label(label)}", shape=ellipse, fillcolor="#fef3c7", color="#b45309"];')
            if src_node:
                lines.append(f'  "{src_node}" -> "{node_name}" [label="/{spi_div}", color="#b45309"];')

        lines.append("  }")
        lines.append("")

    if i2c_consumers:
        lines.append("  subgraph cluster_i2c_io {")
        lines.append('    label="I2C clocks";')
        lines.append('    color="#cbd5e1";')
        lines.append('    style="rounded";')

        for idx, i2c in enumerate(i2c_consumers, start=1):
            node_name = f"i2c_cfg_{idx}"
            inst_name = normalize_instance_name(i2c.instance, f"I2C{idx}")

            freq_id = token_to_freq_id(i2c.clksrc_token) if i2c.clksrc_token else None
            if freq_id is None:
                freq_id = i2c_instance_to_freq_id(i2c.instance)

            src_node = freq_id_to_node(freq_id) if freq_id else None
            src_hz = freqs.get(freq_id) if freq_id else None

            i2c_div = 1
            if src_hz is not None and i2c.speed_hz is not None and i2c.speed_hz > 0:
                i2c_div = src_hz // i2c.speed_hz
                if i2c_div <= 0:
                    i2c_div = 1

            label = inst_name
            if i2c.speed_hz is not None:
                label += f"\n{format_hz(i2c.speed_hz)}"
            elif src_hz is not None:
                label += f"\n{format_hz(src_hz)}"

            lines.append(f'    "{node_name}" [label="{esc_label(label)}", shape=ellipse, fillcolor="#ecfeff", color="#0f766e"];')
            if src_node:
                lines.append(f'  "{src_node}" -> "{node_name}" [label="/{i2c_div}", color="#0f766e"];')

        lines.append("  }")
        lines.append("")

    if adc_consumers:
        lines.append("  subgraph cluster_adc_io {")
        lines.append('    label="ADC clocks";')
        lines.append('    color="#cbd5e1";')
        lines.append('    style="rounded";')

        for idx, adc in enumerate(adc_consumers, start=1):
            node_name = f"adc_cfg_{idx}"
            inst_name = normalize_instance_name(adc.instance, f"ADC{idx}")

            freq_id = token_to_freq_id(adc.clksrc_token) if adc.clksrc_token else None
            src_node = freq_id_to_node(freq_id) if freq_id else None
            src_hz = freqs.get(freq_id) if freq_id else None

            label = inst_name
            if src_hz is not None:
                label += f"\n{format_hz(src_hz)}"

            lines.append(f'    "{node_name}" [label="{esc_label(label)}", shape=ellipse, fillcolor="#f3e8ff", color="#7c3aed"];')
            if src_node:
                lines.append(f'  "{src_node}" -> "{node_name}" [label="/1", color="#7c3aed"];')

        lines.append("  }")
        lines.append("")

    periph_pairs = parse_peripherals(merged_pp)
    if periph_pairs:
        lines.append("  subgraph cluster_peripherals {")
        lines.append('    label="Peripheral clocks (detected enables)";')
        lines.append('    color="#cbd5e1";')
        lines.append('    style="rounded";')

        defined_nodes: set[str] = set()
        periph_edges: set[str] = set()

        for bus, periph in periph_pairs:
            if not periph:
                continue

            periph_sanitized = re.sub(r"[^A-Za-z0-9_]", "_", periph).lower()
            periph_node = f"periph_{periph_sanitized}"

            if periph_node not in defined_nodes:
                defined_nodes.add(periph_node)
                lines.append(f'    "{periph_node}" [label="{esc_label(periph)}", shape=ellipse, fillcolor="#fff7ed"];')

            bus_node, _ = bus_to_node(bus)
            periph_edges.add(f'  "{bus_node}" -> "{periph_node}" [color="#7c3aed"];')

        for edge in sorted(periph_edges):
            lines.append(edge)
        lines.append("  }")
    lines.append("}")

    dot_path = pathlib.Path(args.dot_file)
    write_text(dot_path, "\n".join(lines) + "\n")

    dot_exe: str | None = None
    if args.dot_exe:
        explicit_dot = pathlib.Path(args.dot_exe)
        if explicit_dot.exists():
            dot_exe = str(explicit_dot)
        else:
            dot_exe = shutil.which(args.dot_exe)

    if dot_exe is None:
        dot_exe = shutil.which("dot")

    if dot_exe is not None:
        png_cmd = [dot_exe, "-Tpng", str(dot_path), "-o", str(pathlib.Path(args.png_file))]
        svg_cmd = [dot_exe, "-Tsvg", str(dot_path), "-o", str(pathlib.Path(args.svg_file))]

        png_res = subprocess.run(png_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if png_res.returncode != 0:
            print(
                f"RCC map PNG generation failed for '{args.target_name}'. dot exit code={png_res.returncode}. {png_res.stderr.strip()}",
                file=sys.stderr,
            )

        svg_res = subprocess.run(svg_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if svg_res.returncode != 0:
            print(
                f"RCC map SVG generation failed for '{args.target_name}'. dot exit code={svg_res.returncode}. {svg_res.stderr.strip()}",
                file=sys.stderr,
            )
    else:
        print(f"RCC map: Graphviz 'dot' was not found in PATH. Generated DOT only: {dot_path}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
