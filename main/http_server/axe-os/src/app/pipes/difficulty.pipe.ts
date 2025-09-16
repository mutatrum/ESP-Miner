import { Pipe, PipeTransform } from '@angular/core';

interface SuffixConfig {
  threshold: number;
  divisor: number;
  suffix: string;
}

@Pipe({
  name: 'difficultySuffix',
  standalone: true
})
export class DifficultyPipe implements PipeTransform {
  private readonly suffixes: SuffixConfig[] = [
    { threshold: 1_000_000_000_000_000_000, divisor: 1_000_000_000_000_000, suffix: 'E' },
    { threshold: 1_000_000_000_000_000, divisor: 1_000_000_000_000, suffix: 'P' },
    { threshold: 1_000_000_000_000, divisor: 1_000_000_000, suffix: 'T' },
    { threshold: 1_000_000_000, divisor: 1_000_000, suffix: 'G' },
    { threshold: 1_000_000, divisor: 1_000, suffix: 'M' },
    { threshold: 1_000, divisor: 1_000, suffix: 'k' },
    { threshold: 0, divisor: 1, suffix: '' }
  ];

  transform(value: number | null | undefined): string {
    if (value == null || isNaN(value) || value < 0) {
      return '0';
    }

    const config = this.suffixes.find(s => value >= s.threshold) || this.suffixes[this.suffixes.length - 1];
    const dval = value / config.divisor;

    let result: string;
    if (config.suffix === '') {
      result = Math.floor(dval).toString();
    } else {
      result = dval.toFixed(2);
      result = parseFloat(result).toString();
    }

    return `${result} ${config.suffix}`.trim();
  }
}
