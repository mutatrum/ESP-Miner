import { Component, Input, ElementRef, ViewChild, OnChanges, SimpleChanges, OnDestroy, AfterViewInit, ChangeDetectionStrategy } from '@angular/core';
import { Chart, ChartData, ChartOptions, registerables } from 'chart.js';
import 'chartjs-adapter-moment';

Chart.register(...registerables);

@Component({
  selector: 'app-chart',
  template: `<canvas #chartCanvas class="w-full h-full"></canvas>`,
  styles: [`
    :host {
      display: block;
      position: relative;
      width: 100%;
      height: 100%;
    }
  `],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: true
})
export class ChartComponent implements AfterViewInit, OnChanges, OnDestroy {
  @ViewChild('chartCanvas') canvasRef!: ElementRef<HTMLCanvasElement>;

  @Input() data!: ChartData;
  @Input() options!: ChartOptions;

  public chart?: Chart;

  ngAfterViewInit() {
    this.initChart();
  }

  public refresh() {
    this.chart?.update();
  }

  ngOnChanges(changes: SimpleChanges) {
    if (changes['data'] || changes['options']) {
      if (this.chart) {
        if (changes['data'] && this.data) {
          this.chart.data = this.data;
        }
        if (changes['options'] && this.options) {
          this.chart.options = this.options;
        }
        this.chart.update('none'); // Update without animation for performance
      } else {
        this.initChart();
      }
    }
  }

  private initChart() {
    if (!this.data || !this.options) {
      return;
    }
    if (this.chart) {
      this.chart.destroy();
    }
    const ctx = this.canvasRef.nativeElement.getContext('2d');
    if (ctx) {
      this.chart = new Chart(ctx, {
        type: 'line',
        data: this.data,
        options: this.options
      });
    }
  }

  ngOnDestroy() {
    if (this.chart) {
      this.chart.destroy();
    }
  }
}
