import { Component, Input, HostListener } from '@angular/core';
import { NgClass } from '@angular/common';
import { TooltipDirective } from '../../directives/tooltip.directive';

@Component({
  selector: 'tooltip-icon',
  templateUrl: './tooltip-icon.component.html',
  styleUrls: ['./tooltip-icon.component.scss'],
  imports: [TooltipDirective, NgClass]
})
export class TooltipIconComponent {
  @Input() tooltip: string = '';
  @Input() size: string = 'xs';
  @Input() icon: string = 'pi-question-circle';

  showMobileTooltip = false;
  isMobile = ('ontouchstart' in window) || (navigator.maxTouchPoints > 0);

  get tooltipIconClass(): string {
    return `pi ${this.icon} text-${this.size} pl-1 pr-2 tooltip-icon`;
  }
}
