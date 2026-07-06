import { Component, Injectable, ChangeDetectionStrategy, Inject } from '@angular/core';
import { Observable, Subject } from 'rxjs';
import { Dialog, DialogRef, DIALOG_DATA } from '@angular/cdk/dialog';
import { WifiIconComponent } from '../components/wifi-icon/wifi-icon.component';

interface DialogOption {
  label: string;
  rssi: number;
  value: string;
}

@Injectable({
  providedIn: 'root'
})
export class DialogService {
  constructor(private dialog: Dialog) {}

  open(title: string, options: DialogOption[]): Observable<string> {
    const result = new Subject<string>();

    const ref = this.dialog.open<string>(DialogListComponent, {
      width: '450px',
      panelClass: 'custom-dialog-panel',
      backdropClass: 'custom-dialog-backdrop',
      data: {
        title: title,
        options: options,
        onSelect: (value: string) => {
          result.next(value);
          ref.close();
        }
      }
    });

    ref.closed.subscribe(() => {
      result.complete();
    });

    return result.asObservable();
  }
}

@Component({
  selector: 'app-dialog-list',
  template: `
    <div class="bg-neutral-900 border border-neutral-800 rounded-sm p-6 shadow-2xl relative w-full text-left">
      <div class="flex justify-between items-center mb-4 border-b border-neutral-800 pb-3">
        <h3 class="text-lg font-bold text-white font-sans">{{ data.title }}</h3>
        <button (click)="dialogRef.close()" class="text-neutral-400 hover:text-white transition-colors duration-200 focus:outline-none">
          <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path>
          </svg>
        </button>
      </div>
      <div class="flex flex-col gap-2 max-h-60 overflow-y-auto pr-1">
        @for (option of data.options; track option.value) {
          <button
            (click)="data.onSelect(option.value)"
            class="w-full text-left flex items-center justify-between px-4 py-3 rounded-sm bg-neutral-800/50 hover:bg-neutral-800/80 border border-neutral-700/50 hover:border-neutral-600 text-white font-medium transition-all duration-200 group focus:outline-none focus:ring-1 focus:ring-primary"
            [title]="option.label + ' (' + option.rssi + ' dBm)'"
          >
            <span class="truncate pr-4 text-sm">{{ option.label }}</span>
            <wifi-icon [rssi]="option.rssi" class="flex-shrink-0 text-neutral-400 group-hover:text-primary transition-colors duration-200" />
          </button>
        }
      </div>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.Eager,
  imports: [WifiIconComponent],
  standalone: true
})
export class DialogListComponent {
  constructor(
    public dialogRef: DialogRef<string>,
    @Inject(DIALOG_DATA) public data: { title: string; options: DialogOption[]; onSelect: (val: string) => void }
  ) {}
}
