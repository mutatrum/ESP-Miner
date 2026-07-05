import { Injectable, signal } from '@angular/core';

export interface Toast {
  id: number;
  type: 'success' | 'error' | 'warning' | 'info';
  message: string;
  title?: string;
}

@Injectable({
  providedIn: 'root'
})
export class ToastService {
  private nextId = 0;
  toasts = signal<Toast[]>([]);

  success(message: string, title?: string) {
    this.add('success', message, title);
  }

  error(message: string, title?: string) {
    this.add('error', message, title);
  }

  warning(message: string, title?: string) {
    this.add('warning', message, title);
  }

  info(message: string, title?: string) {
    this.add('info', message, title);
  }

  private add(type: 'success' | 'error' | 'warning' | 'info', message: string, title?: string) {
    const id = this.nextId++;
    const toast: Toast = { id, type, message, title };
    this.toasts.update(current => [...current, toast]);

    setTimeout(() => {
      this.remove(id);
    }, 5000);
  }

  remove(id: number) {
    this.toasts.update(current => current.filter(toast => toast.id !== id));
  }
}

// Export ToastrService for backward compatibility with injecting code
@Injectable({
  providedIn: 'root'
})
export class ToastrService extends ToastService {}
