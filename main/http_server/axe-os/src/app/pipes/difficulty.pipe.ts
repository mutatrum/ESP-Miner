import { Pipe, PipeTransform } from '@angular/core';

interface SuffixConfig {
  threshold: number;
  suffix: string;
}

@Pipe({
  name: 'difficultySuffix',
  standalone: true
})
export class DifficultyPipe implements PipeTransform {
  private readonly suffixes: SuffixConfig[] = [
    { threshold: 1e18, suffix: 'E' },
    { threshold: 1e15, suffix: 'P' },
    { threshold: 1e12, suffix: 'T' },
    { threshold: 1e9,  suffix: 'G' },
    { threshold: 1e6,  suffix: 'M' },
    { threshold: 1e3,  suffix: 'k' },
    { threshold: 1  ,  suffix: ''  }
  ];

  transform(value: number | null | undefined): string {
    if (value == null || isNaN(value) || value < 0) {
      return '0';
    }

    const config = this.suffixes.find(s => value >= s.threshold) || this.suffixes[this.suffixes.length - 1];
    const dval = config.suffix === '' ? value : value / config.threshold;

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
