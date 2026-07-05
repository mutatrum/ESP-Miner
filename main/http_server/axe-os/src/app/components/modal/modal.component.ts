import { Component, Input, ChangeDetectionStrategy } from '@angular/core';

@Component({
    selector: 'app-modal',
    templateUrl: './modal.component.html',
    styleUrls: ['./modal.component.scss'],
    changeDetection: ChangeDetectionStrategy.Eager,
    imports: [],
    standalone: true
})
export class ModalComponent {
  public isVisible = false;

  @Input() headline: string = '';
  @Input() closable: boolean = true;

  constructor() {}
}
